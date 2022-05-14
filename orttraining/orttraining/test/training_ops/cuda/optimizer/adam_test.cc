// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "gtest/gtest.h"

#include "test/providers/provider_test_utils.h"

namespace onnxruntime {
namespace test {

namespace {

struct TensorInfo {
  TensorInfo(VectorInt64 shapes, std::vector<float> values) {
    shapes_ = shapes;
    fp32_values_ = values;

    size_t total_size = 1;
    for (size_t i = 0; i < shapes_.size(); ++i) {
      total_size *= shapes_[i];
    }

    EXPECT_TRUE(fp32_values_.size() == total_size) << "Number of elements mismtach betwen shapes and values."
                                                   << "fp32_values_.size():" << fp32_values_.size()
                                                   << ", total_size: " << total_size;
  }

  template <typename OutT>
  std::vector<OutT> Values() const {
    if (std::is_same<OutT, MLFloat16>::value) {
      std::vector<OutT> fp16_values;
      fp16_values.reserve(fp32_values_.size());
      ConvertFloatToMLFloat16(fp32_values_.data(), reinterpret_cast<onnxruntime::MLFloat16*>(fp16_values.data()), fp32_values_.size());
      return fp16_values;
    } else if (std::is_same<OutT, float>::value) {
      return fp32_values_;
    } else {
      ORT_THROW("Not supported data type.");
    }
  }

  VectorInt64 Shapes() const {
    return shapes_;
  }

  VectorInt64 shapes_;
  std::vector<float> fp32_values_;
};

template <typename T>
struct AdamTestInputOutput {
  AdamTestInputOutput(
      float lr,
      int64_t step,
      const std::vector<TensorInfo>& weight_tensor_infos,
      const std::vector<TensorInfo>& gradient_tensor_infos,
      const std::vector<TensorInfo>& momentum_1_tensor_infos,
      const std::vector<TensorInfo>& momentum_2_tensor_infos,
      const std::vector<TensorInfo>& updated_weight_tensor_infos,
      const std::vector<TensorInfo>& updated_momentum_1_tensor_infos,
      const std::vector<TensorInfo>& updated_momentum_2_tensor_infos) {
    lr_vector.push_back(lr);
    step_vector.push_back(step);

    // Input Sequence tensors.

    for (const TensorInfo& ti : weight_tensor_infos) {
      weight_seq_tensors_.AddTensor(ti.Shapes(), ti.Values<T>());
    }

    for (const TensorInfo& ti : gradient_tensor_infos) {
      gradient_seq_tensors_.AddTensor(ti.Shapes(), ti.Values<T>());
    }

    for (const TensorInfo& ti : momentum_1_tensor_infos) {
      momentum_1_seq_tensors_.AddTensor(ti.Shapes(), ti.Values<T>());
    }

    for (const TensorInfo& ti : momentum_2_tensor_infos) {
      momentum_2_seq_tensors_.AddTensor(ti.Shapes(), ti.Values<T>());
    }

    // Update sequence tensors.

    for (const TensorInfo& ti : updated_weight_tensor_infos) {
      updated_weight_seq_tensors_.AddTensor(ti.Shapes(), ti.Values<T>());
    }

    for (const TensorInfo& ti : updated_momentum_1_tensor_infos) {
      updated_momentum_1_seq_tensors_.AddTensor(ti.Shapes(), ti.Values<T>());
    }

    for (const TensorInfo& ti : updated_momentum_2_tensor_infos) {
      updated_momentum_2_seq_tensors_.AddTensor(ti.Shapes(), ti.Values<T>());
    }
  }

  SeqTensors<T>& WeightSeq() {
    return weight_seq_tensors_;
  }

  SeqTensors<T>& GradientSeq() {
    return gradient_seq_tensors_;
  }

  SeqTensors<T>& Momentum_1_Seq() {
    return momentum_1_seq_tensors_;
  }

  SeqTensors<T>& Momentum_2_Seq() {
    return momentum_2_seq_tensors_;
  }

  SeqTensors<T>& UpdatedWeightSeq() {
    return updated_weight_seq_tensors_;
  }

  SeqTensors<T>& UpdatedMomentum_1_Seq() {
    return updated_momentum_1_seq_tensors_;
  }

  SeqTensors<T>& UpdatedMomentum_2_Seq() {
    return updated_momentum_2_seq_tensors_;
  }

  std::vector<float> lr_vector;
  std::vector<int64_t> step_vector;

 private:
  SeqTensors<T> weight_seq_tensors_;
  SeqTensors<T> gradient_seq_tensors_;
  SeqTensors<T> momentum_1_seq_tensors_;
  SeqTensors<T> momentum_2_seq_tensors_;

  SeqTensors<T> updated_weight_seq_tensors_;
  SeqTensors<T> updated_momentum_1_seq_tensors_;
  SeqTensors<T> updated_momentum_2_seq_tensors_;
};

TEST(AdamTest, TorchAdamWSingleWeightTest_Loop10Steps) {
  size_t total_step = 10;
  float lr = 1e-03;

  // 11 steps of weight values before applying optimization.
  std::vector<std::vector<float>> weights_per_step{
      {-0.18330415, 0.6739549, 0.3117089, 0.42830977, -0.39579117, 0.07424858},
      {-0.18230231, 0.6729482, 0.31270576, 0.4273055, -0.39478722, 0.073247835},
      {-0.18131775, 0.67253, 0.31352443, 0.42674997, -0.39517558, 0.072352484},
      {-0.18059653, 0.6721751, 0.3133565, 0.42615423, -0.39525184, 0.071778126},
      {-0.17977662, 0.6717074, 0.31389162, 0.42545584, -0.39562652, 0.07182311},
      {-0.17979622, 0.6710587, 0.31389156, 0.4246631, -0.39521962, 0.07233689},
      {-0.17989334, 0.67029196, 0.31416565, 0.42445076, -0.3947013, 0.07293851},
      {-0.179972, 0.66955495, 0.31445312, 0.4242259, -0.39403903, 0.07336583},
      {-0.17965378, 0.66875094, 0.31469414, 0.42395133, -0.3932723, 0.07322361},
      {-0.1792204, 0.6680363, 0.3151285, 0.4236793, -0.3924334, 0.0730921},
      {-0.17886575, 0.66727906, 0.31523493, 0.42340374, -0.39180413, 0.07277241},
  };

  // 10 steps of gradient values used to apply optimization.
  std::vector<std::vector<float>> gradients_per_step{
      {-0.18660535, 1.0501877, -0.06538727, 0.78924006, -0.06989894, 0.08311288},
      {-0.14019422, -0.33581918, -0.015272594, -0.11933345, 0.15028854, 0.3035297},
      {0.014209908, 0.052604817, 0.0971713, 0.2388823, -0.056654222, -0.053932328},
      {-0.31329307, 0.3852916, -0.5335826, 0.41899508, 0.117559165, -0.29191104},
      {0.52060837, 0.90293646, 0.45157418, 1.2402161, -0.57814085, -0.977114},
      {0.101783685, 1.272549, -0.43885383, -1.2290779, -0.24205326, -0.41463307},
      {-0.0077258185, 0.28624183, -0.08212745, 0.15442972, -0.4620122, 0.2059978},
      {-0.71347064, 0.98037124, 0.02016977, 0.3170175, -0.6400585, 1.4056847},
      {-0.35443836, 0.0006996552, -0.5025327, 0.11614283, -0.77054685, 0.019379474},
      {0.06727458, 0.71261775, 0.5345064, 0.13658585, 0.27908903, 0.8989311},
  };

  // 11 steps of momentum1 values before applying optimization.
  std::vector<std::vector<float>> momentums_1_per_step{
      {0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
      {-0.018660536, 0.10501877, -0.0065387273, 0.07892401, -0.0069898935, 0.008311288},
      {-0.030813904, 0.060934976, -0.007412114, 0.05909826, 0.00873795, 0.03783313},
      {-0.026311522, 0.06010196, 0.0030462276, 0.077076666, 0.002198733, 0.028656583},
      {-0.055009678, 0.092620924, -0.05061666, 0.111268505, 0.013734776, -0.0034001796},
      {0.002552128, 0.17365249, -0.0003975735, 0.22416328, -0.04545279, -0.10077157},
      {0.012475284, 0.28354216, -0.0442432, 0.07883913, -0.06511284, -0.13215771},
      {0.010455173, 0.2838121, -0.048031624, 0.08639819, -0.10480277, -0.09834216},
      {-0.061937407, 0.353468, -0.041211486, 0.10946012, -0.15832835, 0.05206053},
      {-0.09118751, 0.31819117, -0.08734361, 0.110128395, -0.21955018, 0.04879242},
      {-0.0753413, 0.3576338, -0.025158612, 0.11277413, -0.16968626, 0.13380629},
  };

  // 11 steps of momentum2 values before applying optimization.
  std::vector<std::vector<float>> momentums_2_per_step{
      {0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
      {3.48216e-05, 0.0011028942, 4.2755e-06, 0.0006228999, 4.8859e-06, 6.9078e-06},
      {5.44412e-05, 0.0012145658, 4.5045e-06, 0.0006365175, 2.74676e-05, 9.90311e-05},
      {5.45886e-05, 0.0012161186, 1.39422e-05, 0.0006929458, 3.06499e-05, 0.0001018408},
      {0.0001526866, 0.0013633522, 0.0002986387, 0.0008678096, 4.44394e-05, 0.000186951},
      {0.000423567, 0.0021772832, 0.0005022593, 0.002405078, 0.0003786418, 0.0011415159},
      {0.0004335034, 0.003794487, 0.0006943497, 0.0039133052, 0.0004368529, 0.0013122951},
      {0.0004331296, 0.0038726272, 0.0007004002, 0.00393324, 0.0006498714, 0.001353418},
      {0.0009417369, 0.004829882, 0.0007001066, 0.004029807, 0.0010588964, 0.0033280142},
      {0.0010664216, 0.004825053, 0.0009519457, 0.0040392666, 0.00165158, 0.0033250616},
      {0.0010698811, 0.0053280517, 0.0012366908, 0.0040538833, 0.001727819, 0.004129814},
  };

  ASSERT_TRUE(weights_per_step.size() == total_step + 1);
  ASSERT_TRUE(gradients_per_step.size() == total_step);
  ASSERT_TRUE(momentums_1_per_step.size() == total_step + 1);
  ASSERT_TRUE(momentums_2_per_step.size() == total_step + 1);

  for (size_t step = 0; step < total_step; ++step) {
    OpTester test("Adam", 1, onnxruntime::kMSDomain);
    std::vector<TensorInfo> weight_tensor_infos{TensorInfo({2, 3}, weights_per_step[step])};
    std::vector<TensorInfo> gradient_tensor_infos{TensorInfo({2, 3}, gradients_per_step[step])};
    std::vector<TensorInfo> momentum_1_tensor_infos{TensorInfo({2, 3}, momentums_1_per_step[step])};
    std::vector<TensorInfo> momentum_2_tensor_infos{TensorInfo({2, 3}, momentums_2_per_step[step])};

    std::vector<TensorInfo> updated_weight_tensor_infos{TensorInfo({2, 3}, weights_per_step[step + 1])};
    std::vector<TensorInfo> updated_momentum_1_tensor_infos{TensorInfo({2, 3}, momentums_1_per_step[step + 1])};
    std::vector<TensorInfo> updated_momentum_2_tensor_infos{TensorInfo({2, 3}, momentums_2_per_step[step + 1])};
    AdamTestInputOutput<float> data(
        lr, step, weight_tensor_infos, gradient_tensor_infos, momentum_1_tensor_infos, momentum_2_tensor_infos,
        updated_weight_tensor_infos, updated_momentum_1_tensor_infos, updated_momentum_2_tensor_infos);

    // Default values for Torch AdamW.
    test.AddAttribute("alpha", static_cast<float>(0.9f));
    test.AddAttribute("beta", static_cast<float>(0.999f));
    test.AddAttribute("epsilon", static_cast<float>(1e-8f));
    test.AddAttribute("weight_decay", static_cast<float>(1e-2f));
    test.AddAttribute("adam_mode", static_cast<int64_t>(0));
    test.AddAttribute("correct_bias", static_cast<int64_t>(1));

    // Add test inputs.
    test.AddInput<float>("lr", {}, data.lr_vector);
    test.AddInput<int64_t>("step", {}, data.step_vector);
    test.AddSeqInput("weights", data.WeightSeq());
    test.AddSeqInput("gradients", data.GradientSeq());
    test.AddSeqInput("momentums_1", data.Momentum_1_Seq());
    test.AddSeqInput("momentums_2", data.Momentum_2_Seq());

    // Add test outputs as baseline.
    float param_rtol = 1e-5f;
    float param_atol = 1e-4f;
    test.AddOutput<int64_t>("updated_flag", {}, {1});
    test.AddSeqOutput("updated_weights", data.UpdatedWeightSeq(), param_rtol, param_atol);

    float momentum1_rtol = 1e-3f;
    float momentum1_atol = 1e-6f;
    test.AddSeqOutput("updated_momentums_1", data.UpdatedMomentum_1_Seq(), momentum1_rtol, momentum1_atol);

    float momentum2_rtol = 1e-3f;
    float momentum2_atol = 1e-7f;
    test.AddSeqOutput("updated_momentums_2", data.UpdatedMomentum_2_Seq(), momentum2_rtol, momentum2_atol);

    test.Run();
  }
}

TEST(AdamTest, TorchAdamWMultipleWeightsTest_Loop10Steps) {
  size_t total_step = 10;
  float lr = 1e-03;

  // 11 steps of weight values before applying optimization.
  std::unordered_map<std::string, std::vector<std::vector<float>>> named_weights_per_step{
      {
          "fc1.weight",
          {
              {0.039699044, -0.1405438, 0.6361624, 0.3636222, -0.21526536, 0.3459897},
              {0.040698644, -0.1415424, 0.636156, 0.36361855, -0.2142632, 0.34498626},
              {0.040811654, -0.14246702, 0.6368938, 0.36287078, -0.21388789, 0.3441329},
              {0.040369015, -0.14334096, 0.63773847, 0.3634778, -0.21361604, 0.34342209},
              {0.040421575, -0.14390942, 0.6384292, 0.36397436, -0.21339297, 0.34283924},
              {0.040130284, -0.1445896, 0.6390121, 0.36439353, -0.21320412, 0.34234607},
              {0.04048722, -0.14453566, 0.638672, 0.3640188, -0.21312062, 0.3417903},
              {0.04066716, -0.14445946, 0.63837415, 0.36369106, -0.21294877, 0.34122306},
              {0.040859845, -0.14448948, 0.63874584, 0.36315888, -0.21277392, 0.34052727},
              {0.04108748, -0.14455085, 0.6390751, 0.36268604, -0.21261847, 0.33990923},
              {0.041274574, -0.14464155, 0.6393683, 0.36226365, -0.2124795, 0.3393573},
          },
      },
      {
          "fc1.bias",
          {
              {0.5179555, -0.43510795, -0.12118204},
              {0.5169503, -0.4351036, -0.12218084},
              {0.51600605, -0.4343551, -0.1231803},
              {0.5150944, -0.43356514, -0.1239735},
              {0.5141467, -0.43291724, -0.124623016},
              {0.51373416, -0.432369, -0.12517181},
              {0.513312, -0.43262294, -0.12569128},
              {0.51328874, -0.43284413, -0.12628831},
              {0.51314485, -0.4324527, -0.12694976},
              {0.5130518, -0.43210477, -0.1275368},
              {0.51289135, -0.43179372, -0.12806058},
          },
      },
      {
          "fc2.weight",
          {
              {-0.5107945, 0.045720927, 0.21493156, -0.03950736, -0.35901335, 0.30300942},
              {-0.5097894, 0.04572047, 0.21592939, -0.040506963, -0.35900974, 0.3020064},
              {-0.5088188, 0.046464145, 0.2166485, -0.04150328, -0.35975027, 0.30123964},
              {-0.5078956, 0.047161855, 0.21732824, -0.042312246, -0.3605463, 0.3005772},
              {-0.50694114, 0.047733285, 0.21788464, -0.043107755, -0.36119768, 0.30003402},
              {-0.50652146, 0.048216194, 0.21835458, -0.043949466, -0.3617477, 0.29957443},
              {-0.50599563, 0.047795072, 0.21833022, -0.04463575, -0.36134148, 0.2992945},
              {-0.50579107, 0.047427252, 0.21826117, -0.045370333, -0.36098626, 0.29902303},
              {-0.5054701, 0.04710954, 0.21849227, -0.046180747, -0.36095497, 0.29850963},
              {-0.50520056, 0.046827443, 0.21869715, -0.046999704, -0.3609268, 0.2980535},
              {-0.50488675, 0.046575632, 0.21887977, -0.047773924, -0.36090127, 0.2976461},
          },
      },
      {
          "fc2.bias",
          {
              {0.023823332, 0.53758854},
              {0.024823094, 0.5365832},
              {0.025780898, 0.5355805},
              {0.026681323, 0.53475773},
              {0.027619561, 0.5339611},
              {0.028185476, 0.53309274},
              {0.028722009, 0.53240806},
              {0.02889527, 0.53165853},
              {0.029224215, 0.53083247},
              {0.029507741, 0.53001714},
              {0.029832995, 0.5292876},
          },
      },
  };

  // 10 steps of gradient values used to apply optimization.
  std::unordered_map<std::string, std::vector<std::vector<float>>> named_gradients_per_step{
      {
          "fc1.weight",
          {
              {-0.09586181, 0.17629345, 0.0, 0.0, -0.32529348, 0.04030715},
              {0.068338424, 0.084546804, -0.032574944, 0.0038211804, 0.11912196, 0.22664978},
              {0.14586228, 0.06190881, -0.048464328, -0.08001512, 0.010100813, 0.019007523},
              {-0.13794768, -0.04670714, 0.0, 0.0, 0.0, 0.0},
              {0.17390619, 0.54866207, 0.0, 0.0, 0.0, 0.0},
              {-0.58828557, -0.75700414, 0.17398578, 0.22667544, 0.052081093, 0.06785327},
              {0.16339642, -0.05490701, 0.0, 0.0, -0.07044855, 0.04285267},
              {-0.047095288, 0.189476, -0.504035, 0.18439727, -0.016731672, 0.17345011},
              {-0.080633014, 0.0700092, 0.0, 0.0, 0.0, 0.0},
              {0.022376327, 0.073439084, 0.0, 0.0, 0.0, 0.0},
          },
      },
      {
          "fc1.bias",
          {
              {0.30900255, 0.0, 0.36434337},
              {0.1620592, -0.025315104, 0.37579829},
              {0.13360804, -0.07762981, 0.017482705},
              {0.30951613, 0.0, 0.0},
              {-0.28703725, 0.0, 0.0},
              {0.07509521, 0.16361246, 0.04897593},
              {-0.38377434, 0.0, 0.18683575},
              {0.17580108, -0.458839, 0.19719897},
              {-0.050671168, 0.0, 0.0},
              {0.11543156, 0.0, 0.0},
          },
      },
      {
          "fc2.weight",
          {
              {-0.31826627, 0.0, -0.10227942, 0.7872374, 0.0, 0.2039589},
              {-0.20357697, -0.012987848, -0.0072856103, 0.7206295, 0.021516945, 0.027567107},
              {-0.13538049, -0.117373414, -0.022564847, 0.067673355, 0.057621133, 0.023621686},
              {-0.34123802, 0.0, 0.0, 0.30149734, 0.0, 0.0},
              {0.30987015, 0.0, 0.0, 1.4480324, 0.0, 0.0},
              {-0.23433402, 0.53708684, 0.08742399, -0.13039063, -0.268532, -0.043710135},
              {0.3030513, 0.0, 0.012864593, 0.65198517, 0.0, 0.011254168},
              {-0.22530147, -0.007280223, -0.09456503, 1.1682873, 0.15443315, 0.19558397},
              {0.024412254, 0.0, 0.0, 0.58622336, 0.0, 0.0},
              {-0.11912608, 0.0, 0.0, 0.22673516, 0.0, 0.0},
          },
      },
      {
          "fc2.bias",
          {
              {-0.73845947, 1.7262223},
              {-0.4450062, 1.5997316},
              {-0.27726507, 0.17997742},
              {-0.65889275, 0.5939798},
              {0.39522403, 2.0108204},
              {-0.11679566, -0.36259097},
              {0.63924235, 1.3513944},
              {-0.5523114, 2.2823963},
              {0.025102139, 0.8224809},
              {-0.22954473, 0.0113738775},
          },
      },
  };

  // 11 steps of momentum1 values before applying optimization.
  std::unordered_map<std::string, std::vector<std::vector<float>>>
      named_momentums_1_per_step{
          {
              "fc1.weight",
              {
                  {0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
                  {-0.009586181, 0.017629346, 0.0, 0.0, -0.03252935, 0.004030715},
                  {-0.0017937194, 0.02432109, -0.0032574944, 0.000382118, -0.017364217, 0.026292624},
                  {0.012971881, 0.028079862, -0.0077781775, -0.0076576066, -0.014617712, 0.025564114},
                  {-0.0021200753, 0.020601163, -0.00700036, -0.0068918457, -0.013155941, 0.023007702},
                  {0.015482552, 0.073407255, -0.0063003236, -0.0062026605, -0.011840346, 0.020706931},
                  {-0.044894263, -0.009633889, 0.011728288, 0.01708515, -0.0054482017, 0.025421565},
                  {-0.024065195, -0.014161201, 0.010555458, 0.015376635, -0.011948236, 0.027164675},
                  {-0.026368203, 0.0062025194, -0.040903587, 0.032278698, -0.01242658, 0.04179322},
                  {-0.031794686, 0.012583188, -0.036813226, 0.029050825, -0.011183922, 0.0376139},
                  {-0.026377583, 0.018668776, -0.0331319, 0.026145743, -0.0100655295, 0.033852506},
              },
          },
          {
              "fc1.bias",
              {
                  {0.0, 0.0, 0.0},
                  {0.030900257, 0.0, 0.036434337},
                  {0.04401615, -0.0025315105, 0.070370734},
                  {0.052975338, -0.01004134, 0.06508193},
                  {0.07862942, -0.009037206, 0.05857374},
                  {0.042062752, -0.008133485, 0.052716363},
                  {0.045365997, 0.009041109, 0.05234232},
                  {0.0024519633, 0.008136998, 0.06579167},
                  {0.019786876, -0.0385606, 0.0789324},
                  {0.012741071, -0.03470454, 0.071039155},
                  {0.02301012, -0.031234086, 0.063935235},
              },
          },
          {
              "fc2.weight",
              {
                  {0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
                  {-0.031826627, 0.0, -0.010227942, 0.07872374, 0.0, 0.02039589},
                  {-0.04900166, -0.0012987849, -0.009933708, 0.14291432, 0.0021516946, 0.021113012},
                  {-0.05763954, -0.012906248, -0.011196822, 0.13539022, 0.0076986384, 0.02136388},
                  {-0.085999385, -0.011615623, -0.010077139, 0.15200093, 0.0069287745, 0.019227492},
                  {-0.04641243, -0.0104540605, -0.009069425, 0.28160408, 0.006235897, 0.017304743},
                  {-0.06520459, 0.04430003, 0.0005799162, 0.24040458, -0.021240894, 0.011203255},
                  {-0.028378999, 0.039870027, 0.001808384, 0.28156266, -0.019116804, 0.011208345},
                  {-0.048071247, 0.035155002, -0.007828957, 0.37023512, -0.001761808, 0.029645907},
                  {-0.040822897, 0.0316395, -0.0070460616, 0.39183393, -0.0015856272, 0.026681317},
                  {-0.04865321, 0.028475553, -0.006341455, 0.37532404, -0.0014270644, 0.024013184},
              },
          },
          {
              "fc2.bias",
              {
                  {0.0, 0.0},
                  {-0.073845945, 0.17262223},
                  {-0.11096197, 0.31533316},
                  {-0.12759227, 0.30179757},
                  {-0.18072231, 0.33101577},
                  {-0.12312768, 0.49899623},
                  {-0.122494474, 0.4128375},
                  {-0.046320792, 0.5066932},
                  {-0.09691986, 0.68426347},
                  {-0.08471765, 0.6980852},
                  {-0.09920036, 0.629414},
              },
          },
      };

  // 11 steps of momentum2 values before applying optimization.
  std::unordered_map<std::string, std::vector<std::vector<float>>>
      named_momentums_2_per_step{
          {
              "fc1.weight",
              {
                  {0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
                  {9.1895e-06, 3.10794e-05, 0.0, 0.0, 0.0001058158, 1.6247e-06},
                  {1.38504e-05, 3.81965e-05, 1.0611e-06, 1.46e-08, 0.0001199001, 5.29932e-05},
                  {3.51124e-05, 4.1991e-05, 3.4089e-06, 6.417e-06, 0.0001198822, 5.33015e-05},
                  {5.41068e-05, 4.41305e-05, 3.4054e-06, 6.4106e-06, 0.0001197623, 5.32482e-05},
                  {8.42961e-05, 0.0003451165, 3.402e-06, 6.4042e-06, 0.0001196426, 5.31949e-05},
                  {0.0004302918, 0.0009178267, 3.36697e-05, 5.77795e-05, 0.0001222354, 5.77458e-05},
                  {0.0004565598, 0.0009199237, 3.3636e-05, 5.77218e-05, 0.0001270761, 5.95244e-05},
                  {0.0004583212, 0.0009549049, 0.0002876537, 9.16664e-05, 0.000127229, 8.95498e-05},
                  {0.0004643646, 0.0009588512, 0.000287366, 9.15747e-05, 0.0001271018, 8.94603e-05},
                  {0.000464401, 0.0009632857, 0.0002870786, 9.14832e-05, 0.0001269747, 8.93708e-05},
              },
          },
          {
              "fc1.bias",
              {
                  {0.0, 0.0, 0.0},
                  {9.54826e-05, 0.0, 0.0001327461},
                  {0.0001216503, 6.409e-07, 0.0002738377},
                  {0.0001393797, 6.6666e-06, 0.0002738696},
                  {0.0002350406, 6.6599e-06, 0.0002735957},
                  {0.000317196, 6.6533e-06, 0.0002733221},
                  {0.000322518, 3.34157e-05, 0.0002754474},
                  {0.0004694783, 3.33822e-05, 0.0003100796},
                  {0.0004999148, 0.0002438821, 0.0003486569},
                  {0.0005019825, 0.0002436382, 0.0003483083},
                  {0.000514805, 0.0002433946, 0.00034796},
              },
          },
          {
              "fc2.weight",
              {
                  {0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
                  {0.0001012934, 0.0, 1.04611e-05, 0.0006197428, 0.0, 4.15992e-05},
                  {0.0001426357, 1.687e-07, 1.05037e-05, 0.00113843, 4.63e-07, 4.23176e-05},
                  {0.000160821, 1.3945e-05, 1.10024e-05, 0.0011418712, 3.7827e-06, 4.28332e-05},
                  {0.0002771035, 1.39311e-05, 1.09914e-05, 0.0012316301, 3.7789e-06, 4.27904e-05},
                  {0.000372846, 1.39172e-05, 1.09804e-05, 0.0033271962, 3.7751e-06, 4.27476e-05},
                  {0.0004273856, 0.0003023656, 1.86123e-05, 0.0033408708, 7.58808e-05, 4.46155e-05},
                  {0.0005187982, 0.0003020632, 1.87592e-05, 0.003762615, 7.58049e-05, 4.46975e-05},
                  {0.0005690402, 0.0003018141, 2.7683e-05, 0.0051237475, 9.95787e-05, 8.29059e-05},
                  {0.0005690671, 0.0003015123, 2.76553e-05, 0.005462282, 9.94792e-05, 8.2823e-05},
                  {0.000582689, 0.0003012108, 2.76277e-05, 0.0055082287, 9.93797e-05, 8.27402e-05},
              },
          },
          {
              "fc2.bias",
              {
                  {0.0, 0.0},
                  {0.0005453224, 0.0029798434},
                  {0.0007428076, 0.005536005},
                  {0.0008189408, 0.005562861},
                  {0.0012522616, 0.00591011},
                  {0.0014072113, 0.009947599},
                  {0.0014194453, 0.0100691235},
                  {0.0018266566, 0.011885322},
                  {0.002129878, 0.01708277},
                  {0.002128378, 0.017742163},
                  {0.0021789405, 0.01772455},
              },
          },
      };

  ASSERT_TRUE(named_weights_per_step.size() == 4);
  ASSERT_TRUE(named_gradients_per_step.size() == 4);
  ASSERT_TRUE(named_momentums_1_per_step.size() == 4);
  ASSERT_TRUE(named_momentums_2_per_step.size() == 4);

  ASSERT_TRUE(named_weights_per_step["fc1.weight"].size() == total_step + 1);
  ASSERT_TRUE(named_gradients_per_step["fc1.weight"].size() == total_step);
  ASSERT_TRUE(named_momentums_1_per_step["fc1.weight"].size() == total_step + 1);
  ASSERT_TRUE(named_momentums_2_per_step["fc1.weight"].size() == total_step + 1);

  for (size_t step = 0; step < total_step; ++step) {
    OpTester test("Adam", 1, onnxruntime::kMSDomain);

    // Weights/momentums before applying optimization.
    std::vector<TensorInfo> weight_tensor_infos{
        TensorInfo({2, 3}, named_weights_per_step["fc1.weight"][step]),
        TensorInfo({3}, named_weights_per_step["fc1.bias"][step]),
        TensorInfo({3, 2}, named_weights_per_step["fc2.weight"][step]),
        TensorInfo({2}, named_weights_per_step["fc2.bias"][step]),
    };

    std::vector<TensorInfo> gradient_tensor_infos{
        TensorInfo({2, 3}, named_gradients_per_step["fc1.weight"][step]),
        TensorInfo({3}, named_gradients_per_step["fc1.bias"][step]),
        TensorInfo({3, 2}, named_gradients_per_step["fc2.weight"][step]),
        TensorInfo({2}, named_gradients_per_step["fc2.bias"][step]),
    };

    std::vector<TensorInfo> momentum_1_tensor_infos{
        TensorInfo({2, 3}, named_momentums_1_per_step["fc1.weight"][step]),
        TensorInfo({3}, named_momentums_1_per_step["fc1.bias"][step]),
        TensorInfo({3, 2}, named_momentums_1_per_step["fc2.weight"][step]),
        TensorInfo({2}, named_momentums_1_per_step["fc2.bias"][step]),
    };

    std::vector<TensorInfo> momentum_2_tensor_infos{
        TensorInfo({2, 3}, named_momentums_2_per_step["fc1.weight"][step]),
        TensorInfo({3}, named_momentums_2_per_step["fc1.bias"][step]),
        TensorInfo({3, 2}, named_momentums_2_per_step["fc2.weight"][step]),
        TensorInfo({2}, named_momentums_2_per_step["fc2.bias"][step]),
    };

    // Updated weights/momentums values for validation.
    std::vector<TensorInfo> updated_weight_tensor_infos{
        TensorInfo({2, 3}, named_weights_per_step["fc1.weight"][step + 1]),
        TensorInfo({3}, named_weights_per_step["fc1.bias"][step + 1]),
        TensorInfo({3, 2}, named_weights_per_step["fc2.weight"][step + 1]),
        TensorInfo({2}, named_weights_per_step["fc2.bias"][step + 1]),
    };

    std::vector<TensorInfo> updated_momentum_1_tensor_infos{
        TensorInfo({2, 3}, named_momentums_1_per_step["fc1.weight"][step + 1]),
        TensorInfo({3}, named_momentums_1_per_step["fc1.bias"][step + 1]),
        TensorInfo({3, 2}, named_momentums_1_per_step["fc2.weight"][step + 1]),
        TensorInfo({2}, named_momentums_1_per_step["fc2.bias"][step + 1]),
    };

    std::vector<TensorInfo> updated_momentum_2_tensor_infos{
        TensorInfo({2, 3}, named_momentums_2_per_step["fc1.weight"][step + 1]),
        TensorInfo({3}, named_momentums_2_per_step["fc1.bias"][step + 1]),
        TensorInfo({3, 2}, named_momentums_2_per_step["fc2.weight"][step + 1]),
        TensorInfo({2}, named_momentums_2_per_step["fc2.bias"][step + 1]),
    };

    AdamTestInputOutput<float> data(
        lr, step, weight_tensor_infos, gradient_tensor_infos, momentum_1_tensor_infos, momentum_2_tensor_infos,
        updated_weight_tensor_infos, updated_momentum_1_tensor_infos, updated_momentum_2_tensor_infos);

    // Default values for Torch AdamW.
    test.AddAttribute("alpha", static_cast<float>(0.9f));
    test.AddAttribute("beta", static_cast<float>(0.999f));
    test.AddAttribute("epsilon", static_cast<float>(1e-8f));
    test.AddAttribute("weight_decay", static_cast<float>(1e-2f));
    test.AddAttribute("adam_mode", static_cast<int64_t>(0));
    test.AddAttribute("correct_bias", static_cast<int64_t>(1));

    // Add test inputs.
    test.AddInput<float>("lr", {}, data.lr_vector);
    test.AddInput<int64_t>("step", {}, data.step_vector);
    test.AddSeqInput("weights", data.WeightSeq());
    test.AddSeqInput("gradients", data.GradientSeq());
    test.AddSeqInput("momentums_1", data.Momentum_1_Seq());
    test.AddSeqInput("momentums_2", data.Momentum_2_Seq());

    // Add test outputs as baseline.
    float param_rtol = 1e-5f;
    float param_atol = 1e-4f;
    test.AddOutput<int64_t>("updated_flag", {}, {1});
    test.AddSeqOutput("updated_weights", data.UpdatedWeightSeq(), param_rtol, param_atol);

    float momentum1_rtol = 1e-3f;
    float momentum1_atol = 1e-6f;
    test.AddSeqOutput("updated_momentums_1", data.UpdatedMomentum_1_Seq(), momentum1_rtol, momentum1_atol);

    float momentum2_rtol = 1e-3f;
    float momentum2_atol = 1e-7f;
    test.AddSeqOutput("updated_momentums_2", data.UpdatedMomentum_2_Seq(), momentum2_rtol, momentum2_atol);

    test.Run();
  }
}

TEST(AdamTest, HFAdamWSingleWeightTest_Loop10Steps) {
  size_t total_step = 10;
  float lr = 1e-03;

  // 11 steps of weight values before applying optimization.
  std::vector<std::vector<float>> weights_per_step{
      {-0.18330415, 0.6739549, 0.3117089, 0.42830977, -0.39579117, 0.07424858},
      {-0.18230432, 0.6729549, 0.3127084, 0.4273098, -0.39479163, 0.07324896},
      {-0.18132173, 0.67254347, 0.31352982, 0.42675862, -0.3951839, 0.07235443},
      {-0.18060243, 0.6721953, 0.31336504, 0.42616716, -0.39526412, 0.071780846},
      {-0.17978439, 0.67173433, 0.31390327, 0.42547306, -0.39564267, 0.07182653},
      {-0.17980579, 0.6710924, 0.3139063, 0.4246846, -0.39523974, 0.07234102},
      {-0.17990471, 0.67033243, 0.3141835, 0.4244765, -0.3947254, 0.072943345},
      {-0.17998517, 0.66960216, 0.3144741, 0.42425588, -0.3940671, 0.07337138},
      {-0.17966875, 0.6688048, 0.31471828, 0.42398554, -0.39330435, 0.0732299},
      {-0.17923719, 0.66809684, 0.31515577, 0.4237177, -0.39246938, 0.073099114},
      {-0.17888436, 0.6673463, 0.31526533, 0.42344636, -0.39184403, 0.072780155},
  };

  // 10 steps of gradient values used to apply optimization.
  std::vector<std::vector<float>> gradients_per_step{
      {-0.18660535, 1.0501877, -0.06538727, 0.78924006, -0.06989894, 0.08311288},
      {-0.14019372, -0.33581784, -0.0152721405, -0.11933225, 0.15028848, 0.3035296},
      {0.014210129, 0.05260541, 0.097171515, 0.23888281, -0.056654252, -0.053932384},
      {-0.31330115, 0.3852994, -0.53358376, 0.41899636, 0.11755433, -0.29190654},
      {0.5206307, 0.90298563, 0.45159638, 1.2402637, -0.57814527, -0.9771225},
      {0.10178496, 1.272584, -0.4388461, -1.229053, -0.2420594, -0.41463128},
      {-0.007730415, 0.28626472, -0.082126565, 0.15444252, -0.46201625, 0.20600216},
      {-0.71351, 0.98043907, 0.020160869, 0.3170406, -0.64007777, 1.4057107},
      {-0.35445014, 0.0007170309, -0.5025207, 0.11615195, -0.77056587, 0.01938322},
      {0.06728999, 0.71267974, 0.53454334, 0.13664238, 0.27906644, 0.8989229},
  };

  // 11 steps of momentum1 values before applying optimization.
  std::vector<std::vector<float>> momentums_1_per_step{
      {0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
      {-0.018660536, 0.10501877, -0.0065387273, 0.07892401, -0.0069898935, 0.008311288},
      {-0.030813852, 0.06093511, -0.007412069, 0.05909838, 0.008737944, 0.03783312},
      {-0.026311453, 0.06010214, 0.0030462903, 0.07707683, 0.0021987236, 0.02865657},
      {-0.055010427, 0.09262186, -0.050616715, 0.11126879, 0.013734283, -0.003399741},
      {0.0025536926, 0.17365824, -0.0003954024, 0.22416827, -0.04545367, -0.100772016},
      {0.01247682, 0.2835508, -0.04424047, 0.078846134, -0.065114245, -0.13215794},
      {0.010456095, 0.28382218, -0.04802908, 0.08640577, -0.10480444, -0.09834193},
      {-0.061940514, 0.3534839, -0.041210085, 0.10946925, -0.15833177, 0.05206334},
      {-0.09119148, 0.3182072, -0.087341145, 0.11013751, -0.21955517, 0.048795324},
      {-0.075343326, 0.35765445, -0.025152696, 0.11278799, -0.16969301, 0.13380808},
  };

  // 11 steps of momentum2 values before applying optimization.
  std::vector<std::vector<float>> momentums_2_per_step{
      {0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
      {3.48216e-05, 0.0011028942, 4.2755e-06, 0.0006228999, 4.8859e-06, 6.9078e-06},
      {5.4441e-05, 0.0012145649, 4.5045e-06, 0.0006365172, 2.74676e-05, 9.90311e-05},
      {5.45885e-05, 0.0012161176, 1.39423e-05, 0.0006929458, 3.06498e-05, 0.0001018407},
      {0.0001526915, 0.0013633572, 0.00029864, 0.0008678107, 4.44382e-05, 0.0001869483},
      {0.0004235952, 0.002177377, 0.0005022806, 0.0024051971, 0.0003786457, 0.0011415298},
      {0.0004335318, 0.0037946696, 0.0006943642, 0.0039133634, 0.0004368598, 0.0013123073},
      {0.000433158, 0.0038728225, 0.0007004146, 0.0039333026, 0.000649882, 0.001353432},
      {0.0009418214, 0.0048302105, 0.0007001207, 0.004029884, 0.0010589317, 0.0033281012},
      {0.0010665145, 0.0048253806, 0.0009519476, 0.0040393453, 0.0016516446, 0.003325149},
      {0.001069976, 0.005328468, 0.0012367324, 0.0040539773, 0.0017278712, 0.004129886},
  };

  ASSERT_TRUE(weights_per_step.size() == total_step + 1);
  ASSERT_TRUE(gradients_per_step.size() == total_step);
  ASSERT_TRUE(momentums_1_per_step.size() == total_step + 1);
  ASSERT_TRUE(momentums_2_per_step.size() == total_step + 1);

  for (size_t step = 0; step < total_step; ++step) {
    OpTester test("Adam", 1, onnxruntime::kMSDomain);
    std::vector<TensorInfo> weight_tensor_infos{TensorInfo({2, 3}, weights_per_step[step])};
    std::vector<TensorInfo> gradient_tensor_infos{TensorInfo({2, 3}, gradients_per_step[step])};
    std::vector<TensorInfo> momentum_1_tensor_infos{TensorInfo({2, 3}, momentums_1_per_step[step])};
    std::vector<TensorInfo> momentum_2_tensor_infos{TensorInfo({2, 3}, momentums_2_per_step[step])};

    std::vector<TensorInfo> updated_weight_tensor_infos{TensorInfo({2, 3}, weights_per_step[step + 1])};
    std::vector<TensorInfo> updated_momentum_1_tensor_infos{TensorInfo({2, 3}, momentums_1_per_step[step + 1])};
    std::vector<TensorInfo> updated_momentum_2_tensor_infos{TensorInfo({2, 3}, momentums_2_per_step[step + 1])};
    AdamTestInputOutput<float> data(
        lr, step, weight_tensor_infos, gradient_tensor_infos, momentum_1_tensor_infos, momentum_2_tensor_infos,
        updated_weight_tensor_infos, updated_momentum_1_tensor_infos, updated_momentum_2_tensor_infos);

    // Default values for HF AdamW.
    test.AddAttribute("alpha", static_cast<float>(0.9f));
    test.AddAttribute("beta", static_cast<float>(0.999f));
    test.AddAttribute("epsilon", static_cast<float>(1e-6f));
    test.AddAttribute("weight_decay", static_cast<float>(0.0f));
    test.AddAttribute("adam_mode", static_cast<int64_t>(0));
    test.AddAttribute("correct_bias", static_cast<int64_t>(1));

    // Add test inputs.
    test.AddInput<float>("lr", {}, data.lr_vector);
    test.AddInput<int64_t>("step", {}, data.step_vector);
    test.AddSeqInput("weights", data.WeightSeq());
    test.AddSeqInput("gradients", data.GradientSeq());
    test.AddSeqInput("momentums_1", data.Momentum_1_Seq());
    test.AddSeqInput("momentums_2", data.Momentum_2_Seq());

    // Add test outputs as baseline.
    float param_rtol = 1e-5f;
    float param_atol = 1e-4f;
    test.AddOutput<int64_t>("updated_flag", {}, {1});
    test.AddSeqOutput("updated_weights", data.UpdatedWeightSeq(), param_rtol, param_atol);

    float momentum1_rtol = 1e-3f;
    float momentum1_atol = 1e-6f;
    test.AddSeqOutput("updated_momentums_1", data.UpdatedMomentum_1_Seq(), momentum1_rtol, momentum1_atol);

    float momentum2_rtol = 1e-3f;
    float momentum2_atol = 1e-7f;
    test.AddSeqOutput("updated_momentums_2", data.UpdatedMomentum_2_Seq(), momentum2_rtol, momentum2_atol);

    test.Run();
  }
}

TEST(AdamTest, HFAdamWMultipleWeightsTest_Loop10Steps) {
  size_t total_step = 10;
  float lr = 1e-03;

  // 11 steps of weight values before applying optimization.
  std::unordered_map<std::string, std::vector<std::vector<float>>> named_weights_per_step{
      {
          "fc1.weight",
          {
              {0.039699044, -0.1405438, 0.6361624, 0.3636222, -0.21526536, 0.3459897},
              {0.040698715, -0.14154363, 0.6361624, 0.3636222, -0.21426547, 0.3449905},
              {0.040812083, -0.14246951, 0.6369058, 0.36288416, -0.21389234, 0.34414068},
              {0.040369906, -0.14334475, 0.63775635, 0.36349458, -0.21362267, 0.3434334},
              {0.040422853, -0.14391455, 0.63845307, 0.3639946, -0.21340178, 0.34285408},
              {0.040131994, -0.14459613, 0.63904196, 0.36441723, -0.21321507, 0.3423644},
              {0.040489312, -0.1445436, 0.6387083, 0.36404616, -0.21313372, 0.34181213},
              {0.040669657, -0.14446883, 0.6384169, 0.36372212, -0.21296401, 0.3412484},
              {0.040862735, -0.14450029, 0.63879496, 0.36319363, -0.21279131, 0.3405561},
              {0.041090745, -0.14456306, 0.6391306, 0.36272448, -0.21263799, 0.3399415},
              {0.041278224, -0.14465518, 0.6394301, 0.36230576, -0.21250115, 0.339393},
          },
      },
      {
          "fc1.bias",
          {
              {0.5179555, -0.43510795, -0.12118204},
              {0.5169556, -0.43510795, -0.12218197},
              {0.5160166, -0.43436474, -0.123182595},
              {0.5151102, -0.43357942, -0.12397698},
              {0.5141677, -0.43293613, -0.1246277},
              {0.5137603, -0.4323924, -0.12517771},
              {0.5133432, -0.4326506, -0.1256984},
              {0.5133251, -0.43287608, -0.12629665},
              {0.51318634, -0.432489, -0.12695934},
              {0.5130984, -0.43214542, -0.12754764},
              {0.512943, -0.43183872, -0.12807268},
          },
      },
      {
          "fc2.weight",
          {
              {-0.5107945, 0.045720927, 0.21493156, -0.03950736, -0.35901335, 0.30300942},
              {-0.5097946, 0.045720927, 0.21593124, -0.04050732, -0.35901335, 0.30200958},
              {-0.5088292, 0.04646326, 0.21665226, -0.041504018, -0.35975638, 0.30124596},
              {-0.5079111, 0.047161248, 0.21733399, -0.04231338, -0.3605556, 0.30058664},
              {-0.50696176, 0.047732998, 0.21789241, -0.043109298, -0.36121026, 0.30004656},
              {-0.50654715, 0.048216257, 0.21836439, -0.043951426, -0.3617636, 0.29959008},
              {-0.5060264, 0.047795646, 0.2183422, -0.04463814, -0.36136106, 0.2993132},
              {-0.50582683, 0.047428325, 0.21827532, -0.04537316, -0.3610095, 0.29904476},
              {-0.50551087, 0.04711111, 0.21850857, -0.046184022, -0.36098182, 0.29853436},
              {-0.50524634, 0.046829507, 0.21871562, -0.04700344, -0.36095726, 0.29808128},
              {-0.5049375, 0.046578184, 0.2189004, -0.047778137, -0.36093533, 0.29767692},
          },
      },
      {
          "fc2.bias",
          {
              {0.023823332, 0.53758854},
              {0.02482329, 0.53658855},
              {0.025781307, 0.53559124},
              {0.026681963, 0.5347738},
              {0.027620444, 0.5339826},
              {0.028186643, 0.53311956},
              {0.028723473, 0.5324403},
              {0.028897049, 0.5316961},
              {0.029226314, 0.5308753},
              {0.029510176, 0.5300653},
              {0.029835783, 0.52934104},
          },
      },
  };

  // 10 steps of gradient values used to apply optimization.
  std::unordered_map<std::string, std::vector<std::vector<float>>> named_gradients_per_step{
      {
          "fc1.weight",
          {
              {-0.09586181, 0.17629345, 0.0, 0.0, -0.32529348, 0.04030715},
              {0.06834118, 0.08454809, -0.03257603, 0.003821307, 0.11912338, 0.22665249},
              {0.14587241, 0.061909888, -0.048467256, -0.08001699, 0.01010122, 0.019008288},
              {-0.13795517, -0.046709955, 0.0, 0.0, 0.0, 0.0},
              {0.17391214, 0.5486637, 0.0, 0.0, 0.0, 0.0},
              {-0.5883157, -0.7570428, 0.17399102, 0.22668226, 0.05208155, 0.06785387},
              {0.1634007, -0.054908697, 0.0, 0.0, -0.07045339, 0.042855613},
              {-0.04708763, 0.18948916, -0.50407434, 0.18441166, -0.01673333, 0.1734673},
              {-0.08061128, 0.06998594, 0.0, 0.0, 0.0, 0.0},
              {0.022378631, 0.07342364, 0.0, 0.0, 0.0, 0.0},
          },
      },
      {
          "fc1.bias",
          {
              {0.30900255, 0.0, 0.36434337},
              {0.16206324, -0.025315944, 0.37580276},
              {0.13361552, -0.077632844, 0.01748341},
              {0.30953294, 0.0, 0.0},
              {-0.287035, 0.0, 0.0},
              {0.075108826, 0.16361739, 0.04897636},
              {-0.38378465, 0.0, 0.18684858},
              {0.17582831, -0.4588748, 0.19721851},
              {-0.050648615, 0.0, 0.0},
              {0.11546661, 0.0, 0.0},
          },
      },
      {
          "fc2.weight",
          {
              {-0.31826627, 0.0, -0.10227942, 0.7872374, 0.0, 0.2039589},
              {-0.2035816, -0.012988876, -0.0072859353, 0.72063875, 0.021517584, 0.02756824},
              {-0.13538884, -0.11738108, -0.022565873, 0.06767718, 0.057625376, 0.023622716},
              {-0.34125668, 0.0, 0.0, 0.3015144, 0.0, 0.0},
              {0.30986175, 0.0, 0.0, 1.4481032, 0.0, 0.0},
              {-0.2343559, 0.5371115, 0.087428816, -0.1303899, -0.26854572, -0.04371277},
              {0.30305678, 0.0, 0.01286526, 0.6520397, 0.0, 0.011255648},
              {-0.22533442, -0.007282924, -0.09457447, 1.1683875, 0.15443821, 0.19560373},
              {0.02437459, 0.0, 0.0, 0.5862967, 0.0, 0.0},
              {-0.11917039, 0.0, 0.0, 0.22678037, 0.0, 0.0},
          },
      },
      {
          "fc2.bias",
          {
              {-0.73845947, 1.7262223},
              {-0.44501105, 1.5997354},
              {-0.2772748, 0.17998222},
              {-0.65890837, 0.5939946},
              {0.39519566, 2.0108397},
              {-0.11681646, -0.36257744},
              {0.6392148, 1.3514249},
              {-0.55234224, 2.2824335},
              {0.025046706, 0.8225188},
              {-0.22959742, 0.0114171505},
          },
      },
  };

  // 11 steps of momentum1 values before applying optimization.
  std::unordered_map<std::string, std::vector<std::vector<float>>>
      named_momentums_1_per_step{
          {
              "fc1.weight",
              {
                  {0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
                  {-0.009586181, 0.017629346, 0.0, 0.0, -0.03252935, 0.004030715},
                  {-0.0017934436, 0.024321219, -0.003257603, 0.0003821307, -0.017364075, 0.026292892},
                  {0.012973143, 0.028080087, -0.007778568, -0.0076577817, -0.0146175455, 0.025564432},
                  {-0.0021196895, 0.02060108, -0.0070007113, -0.006892003, -0.013155791, 0.023007989},
                  {0.015483495, 0.073407345, -0.00630064, -0.006202803, -0.011840211, 0.02070719},
                  {-0.044896428, -0.009637676, 0.011728527, 0.017085703, -0.005448035, 0.025421858},
                  {-0.024066713, -0.0141647775, 0.010555674, 0.015377133, -0.011948571, 0.027165232},
                  {-0.026368804, 0.0062006167, -0.040907327, 0.032280587, -0.012427047, 0.041795436},
                  {-0.03179305, 0.012579149, -0.036816593, 0.029052528, -0.011184342, 0.03761589},
                  {-0.02637588, 0.018663598, -0.033134934, 0.026147274, -0.010065908, 0.033854302},
              },
          },
          {
              "fc1.bias",
              {
                  {0.0, 0.0, 0.0},
                  {0.030900257, 0.0, 0.036434337},
                  {0.044016555, -0.0025315944, 0.07037118},
                  {0.052976448, -0.010041719, 0.0650824},
                  {0.078632094, -0.009037547, 0.058574155},
                  {0.042065386, -0.008133792, 0.052716743},
                  {0.04536973, 0.009041326, 0.0523427},
                  {0.0024542885, 0.0081371935, 0.06579329},
                  {0.01979169, -0.038564004, 0.07893581},
                  {0.012747658, -0.034707602, 0.071042225},
                  {0.023019554, -0.031236842, 0.063938},
              },
          },
          {
              "fc2.weight",
              {
                  {0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
                  {-0.031826627, 0.0, -0.010227942, 0.07872374, 0.0, 0.02039589},
                  {-0.049002126, -0.0012988875, -0.009933741, 0.14291525, 0.0021517584, 0.021113126},
                  {-0.057640795, -0.012907107, -0.011196953, 0.13539144, 0.00769912, 0.021364084},
                  {-0.08600238, -0.011616397, -0.010077258, 0.15200374, 0.006929208, 0.019227674},
                  {-0.046415962, -0.010454757, -0.009069531, 0.28161368, 0.0062362873, 0.017304907},
                  {-0.065209955, 0.04430187, 0.000580304, 0.24041331, -0.021241914, 0.011203138},
                  {-0.02838328, 0.03987168, 0.0018087996, 0.28157595, -0.019117722, 0.011208389},
                  {-0.048078395, 0.035156216, -0.007829527, 0.3702571, -0.0017621284, 0.029647924},
                  {-0.040833097, 0.031640593, -0.0070465743, 0.39186105, -0.0015859156, 0.02668313},
                  {-0.048666827, 0.02847653, -0.0063419165, 0.37535298, -0.001427324, 0.024014816},
              },
          },
          {
              "fc2.bias",
              {
                  {0.0, 0.0},
                  {-0.073845945, 0.17262223},
                  {-0.11096244, 0.31533355},
                  {-0.12759368, 0.3017984},
                  {-0.18072514, 0.331018},
                  {-0.12313307, 0.49900016},
                  {-0.1225014, 0.41284242},
                  {-0.04632978, 0.50670063},
                  {-0.096931025, 0.6842739},
                  {-0.08473325, 0.6980984},
                  {-0.09921966, 0.6294303},
              },
          },
      };

  // 11 steps of momentum2 values before applying optimization.
  std::unordered_map<std::string, std::vector<std::vector<float>>>
      named_momentums_2_per_step{
          {
              "fc1.weight",
              {
                  {0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
                  {9.1895e-06, 3.10794e-05, 0.0, 0.0, 0.0001058158, 1.6247e-06},
                  {1.38508e-05, 3.81967e-05, 1.0612e-06, 1.46e-08, 0.0001199004, 5.29944e-05},
                  {3.51157e-05, 4.19913e-05, 3.4092e-06, 6.4173e-06, 0.0001198826, 5.33027e-05},
                  {5.41122e-05, 4.41312e-05, 3.4058e-06, 6.4109e-06, 0.0001197627, 5.32494e-05},
                  {8.43036e-05, 0.0003451189, 3.4024e-06, 6.4045e-06, 0.0001196429, 5.31962e-05},
                  {0.0004303346, 0.0009178877, 3.36719e-05, 5.77829e-05, 0.0001222358, 5.77471e-05},
                  {0.0004566041, 0.0009199848, 3.36382e-05, 5.77251e-05, 0.0001270772, 5.9526e-05},
                  {0.0004583648, 0.000954971, 0.0002876955, 9.16751e-05, 0.0001272302, 8.95574e-05},
                  {0.0004644046, 0.0009589141, 0.0002874078, 9.15834e-05, 0.0001271029, 8.94678e-05},
                  {0.000464441, 0.0009633462, 0.0002871204, 9.14918e-05, 0.0001269758, 8.93783e-05},
              },
          },
          {
              "fc1.bias",
              {
                  {0.0, 0.0, 0.0},
                  {9.54826e-05, 0.0, 0.0001327461},
                  {0.0001216516, 6.409e-07, 0.0002738411},
                  {0.0001393831, 6.6671e-06, 0.0002738729},
                  {0.0002350543, 6.6604e-06, 0.000273599},
                  {0.0003172084, 6.6538e-06, 0.0002733254},
                  {0.0003225325, 3.34178e-05, 0.0002754508},
                  {0.0004695006, 3.33844e-05, 0.0003100878},
                  {0.0004999468, 0.0002439171, 0.0003486728},
                  {0.0005020121, 0.0002436732, 0.0003483242},
                  {0.0005148426, 0.0002434295, 0.0003479759},
              },
          },
          {
              "fc2.weight",
              {
                  {0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
                  {0.0001012934, 0.0, 1.04611e-05, 0.0006197428, 0.0, 4.15992e-05},
                  {0.0001426376, 1.687e-07, 1.05037e-05, 0.0011384432, 4.63e-07, 4.23176e-05},
                  {0.0001608251, 1.39469e-05, 1.10024e-05, 0.0011418851, 3.7832e-06, 4.28334e-05},
                  {0.0002771204, 1.39329e-05, 1.09914e-05, 0.0012316541, 3.7794e-06, 4.27905e-05},
                  {0.0003728576, 1.3919e-05, 1.09804e-05, 0.0033274253, 3.7757e-06, 4.27477e-05},
                  {0.0004274074, 0.0003023939, 1.86132e-05, 0.0033410995, 7.58887e-05, 4.46158e-05},
                  {0.0005188234, 0.0003020915, 1.87601e-05, 0.003762914, 7.58128e-05, 4.46979e-05},
                  {0.0005690802, 0.0003018424, 2.76857e-05, 0.0051242807, 9.95882e-05, 8.2914e-05},
                  {0.0005691052, 0.0003015406, 2.7658e-05, 0.0054629003, 9.94886e-05, 8.28311e-05},
                  {0.0005827378, 0.000301239, 2.76304e-05, 0.005508867, 9.93891e-05, 8.27482e-05},
              },
          },
          {
              "fc2.bias",
              {
                  {0.0, 0.0},
                  {0.0005453224, 0.0029798434},
                  {0.000742812, 0.005536017},
                  {0.0008189504, 0.005562875},
                  {0.0012522918, 0.005910142},
                  {0.0014072192, 0.009947708},
                  {0.0014194581, 0.010069223},
                  {0.0018266342, 0.011885503},
                  {0.0021298896, 0.017083121},
                  {0.0021283869, 0.017742576},
                  {0.0021789735, 0.017724965},
              },
          },
      };

  ASSERT_TRUE(named_weights_per_step.size() == 4);
  ASSERT_TRUE(named_gradients_per_step.size() == 4);
  ASSERT_TRUE(named_momentums_1_per_step.size() == 4);
  ASSERT_TRUE(named_momentums_2_per_step.size() == 4);

  ASSERT_TRUE(named_weights_per_step["fc1.weight"].size() == total_step + 1);
  ASSERT_TRUE(named_gradients_per_step["fc1.weight"].size() == total_step);
  ASSERT_TRUE(named_momentums_1_per_step["fc1.weight"].size() == total_step + 1);
  ASSERT_TRUE(named_momentums_2_per_step["fc1.weight"].size() == total_step + 1);

  for (size_t step = 0; step < total_step; ++step) {
    OpTester test("Adam", 1, onnxruntime::kMSDomain);

    // Weights/momentums before applying optimization.
    std::vector<TensorInfo> weight_tensor_infos{
        TensorInfo({2, 3}, named_weights_per_step["fc1.weight"][step]),
        TensorInfo({3}, named_weights_per_step["fc1.bias"][step]),
        TensorInfo({3, 2}, named_weights_per_step["fc2.weight"][step]),
        TensorInfo({2}, named_weights_per_step["fc2.bias"][step]),
    };

    std::vector<TensorInfo> gradient_tensor_infos{
        TensorInfo({2, 3}, named_gradients_per_step["fc1.weight"][step]),
        TensorInfo({3}, named_gradients_per_step["fc1.bias"][step]),
        TensorInfo({3, 2}, named_gradients_per_step["fc2.weight"][step]),
        TensorInfo({2}, named_gradients_per_step["fc2.bias"][step]),
    };

    std::vector<TensorInfo> momentum_1_tensor_infos{
        TensorInfo({2, 3}, named_momentums_1_per_step["fc1.weight"][step]),
        TensorInfo({3}, named_momentums_1_per_step["fc1.bias"][step]),
        TensorInfo({3, 2}, named_momentums_1_per_step["fc2.weight"][step]),
        TensorInfo({2}, named_momentums_1_per_step["fc2.bias"][step]),
    };

    std::vector<TensorInfo> momentum_2_tensor_infos{
        TensorInfo({2, 3}, named_momentums_2_per_step["fc1.weight"][step]),
        TensorInfo({3}, named_momentums_2_per_step["fc1.bias"][step]),
        TensorInfo({3, 2}, named_momentums_2_per_step["fc2.weight"][step]),
        TensorInfo({2}, named_momentums_2_per_step["fc2.bias"][step]),
    };

    // Updated weights/momentums values for validation.
    std::vector<TensorInfo> updated_weight_tensor_infos{
        TensorInfo({2, 3}, named_weights_per_step["fc1.weight"][step + 1]),
        TensorInfo({3}, named_weights_per_step["fc1.bias"][step + 1]),
        TensorInfo({3, 2}, named_weights_per_step["fc2.weight"][step + 1]),
        TensorInfo({2}, named_weights_per_step["fc2.bias"][step + 1]),
    };

    std::vector<TensorInfo> updated_momentum_1_tensor_infos{
        TensorInfo({2, 3}, named_momentums_1_per_step["fc1.weight"][step + 1]),
        TensorInfo({3}, named_momentums_1_per_step["fc1.bias"][step + 1]),
        TensorInfo({3, 2}, named_momentums_1_per_step["fc2.weight"][step + 1]),
        TensorInfo({2}, named_momentums_1_per_step["fc2.bias"][step + 1]),
    };

    std::vector<TensorInfo> updated_momentum_2_tensor_infos{
        TensorInfo({2, 3}, named_momentums_2_per_step["fc1.weight"][step + 1]),
        TensorInfo({3}, named_momentums_2_per_step["fc1.bias"][step + 1]),
        TensorInfo({3, 2}, named_momentums_2_per_step["fc2.weight"][step + 1]),
        TensorInfo({2}, named_momentums_2_per_step["fc2.bias"][step + 1]),
    };

    AdamTestInputOutput<float> data(
        lr, step, weight_tensor_infos, gradient_tensor_infos, momentum_1_tensor_infos, momentum_2_tensor_infos,
        updated_weight_tensor_infos, updated_momentum_1_tensor_infos, updated_momentum_2_tensor_infos);

    // Default values for HF AdamW.
    test.AddAttribute("alpha", static_cast<float>(0.9f));
    test.AddAttribute("beta", static_cast<float>(0.999f));
    test.AddAttribute("epsilon", static_cast<float>(1e-6f));
    test.AddAttribute("weight_decay", static_cast<float>(0.0f));
    test.AddAttribute("adam_mode", static_cast<int64_t>(0));
    test.AddAttribute("correct_bias", static_cast<int64_t>(1));

    // Add test inputs.
    test.AddInput<float>("lr", {}, data.lr_vector);
    test.AddInput<int64_t>("step", {}, data.step_vector);
    test.AddSeqInput("weights", data.WeightSeq());
    test.AddSeqInput("gradients", data.GradientSeq());
    test.AddSeqInput("momentums_1", data.Momentum_1_Seq());
    test.AddSeqInput("momentums_2", data.Momentum_2_Seq());

    // Add test outputs as baseline.
    float param_rtol = 1e-5f;
    float param_atol = 1e-4f;
    test.AddOutput<int64_t>("updated_flag", {}, {1});
    test.AddSeqOutput("updated_weights", data.UpdatedWeightSeq(), param_rtol, param_atol);

    float momentum1_rtol = 1e-3f;
    float momentum1_atol = 1e-6f;
    test.AddSeqOutput("updated_momentums_1", data.UpdatedMomentum_1_Seq(), momentum1_rtol, momentum1_atol);

    float momentum2_rtol = 1e-3f;
    float momentum2_atol = 1e-7f;
    test.AddSeqOutput("updated_momentums_2", data.UpdatedMomentum_2_Seq(), momentum2_rtol, momentum2_atol);

    test.Run();
  }
}

}  // namespace
}  // namespace test
}  // namespace onnxruntime
