/* Copyright (C) 2016-2018 Alibaba Group Holding Limited

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "xdl/core/lib/status.h"
#include "xdl/core/framework/op_kernel.h"
#include "xdl/core/framework/op_define.h"
#include "xdl/core/framework/op_registry.h"
#include "xdl/core/ops/ps_ops/define_op.h"
#include "xdl/core/ops/ps_ops/convert_utils.h"
#include "xdl/core/ops/ps_ops/client.h"
#include "xdl/core/ops/ps_ops/var_type.h"

namespace xdl {

template <typename T>
class PsSparseApplyFtrlOp : public xdl::OpKernelAsync {
 public:
  Status Init(OpKernelConstruction* ctx) override {
    XDL_CHECK_STATUS(ctx->GetAttr("var_name", &var_name_));
    XDL_CHECK_STATUS(XdlGetVarType(ctx, &var_type_));
    return Status::Ok();
  }

  void Compute(OpKernelContext* ctx, Callback done) override {
    ps::client::BaseClient* client;
    XDL_CHECK_STATUS_ASYNC(GetClient(&client), done);
    Tensor t_lr;
    XDL_CHECK_STATUS_ASYNC(ctx->GetInput(0, &t_lr), done);
    double lr = t_lr.Scalar<double>();
    Tensor t_lr_power;
    XDL_CHECK_STATUS_ASYNC(ctx->GetInput(1, &t_lr_power), done);
    double lr_power = t_lr_power.Scalar<double>();
    Tensor t_init_acc;
    XDL_CHECK_STATUS_ASYNC(ctx->GetInput(2, &t_init_acc), done);
    double init_acc = t_init_acc.Scalar<double>();
    Tensor t_l1_reg;
    XDL_CHECK_STATUS_ASYNC(ctx->GetInput(3, &t_l1_reg), done);
    double l1_reg = t_l1_reg.Scalar<double>();
    Tensor t_l2_reg;
    XDL_CHECK_STATUS_ASYNC(ctx->GetInput(4, &t_l2_reg), done);
    double l2_reg = t_l2_reg.Scalar<double>();
    Tensor grad;
    XDL_CHECK_STATUS_ASYNC(ctx->GetInput(5, &grad), done);
    Tensor indices;
    XDL_CHECK_STATUS_ASYNC(ctx->GetInput(6, &indices), done);    
    ps::Tensor convert_grad;
    XDL_CHECK_STATUS_ASYNC(
        XDL2PS::ConvertTensor(grad, &convert_grad),
        done);
    ps::Tensor convert_indices;
    XDL_CHECK_STATUS_ASYNC(
        XDL2PS::ConvertTensor(indices, &convert_indices),
        done);
    auto cb = [ctx, done](const ps::Status& st) {
      XDL_CHECK_STATUS_ASYNC(PS2XDL::ConvertStatus(st), done);
      done(Status::Ok());
    };

    switch(var_type_) {
    case VarType::kIndex:
      client->SparsePush(
          var_name_, 
          convert_indices, 
          "FtrlUpdater", 
          client->Args(convert_grad, lr, lr_power, init_acc, l1_reg, l2_reg), 
          cb);
      break;
    case VarType::kHash:
      client->HashPush(
          var_name_, 
          convert_indices, 
          "FtrlUpdater", 
          client->Args(convert_grad, lr, lr_power, init_acc, l1_reg, l2_reg), 
          cb);      
      break;
    default:
      XDL_CHECK_COND_ASYNC(
          false, 
          Status::ArgumentError("unsupported vartype"),
          done);
    }
  }

 private:
  std::string var_name_;
  VarType var_type_;
};

XDL_DEFINE_OP(PsSparseApplyFtrlOp)
  .Input("learning_rate", DataType::kDouble)
  .Input("learning_rate_power", DataType::kDouble)
  .Input("initial_accumulator_value", DataType::kDouble)
  .Input("l1_reg", DataType::kDouble)
  .Input("l2_reg", DataType::kDouble)
  .Input("grad", DataType::kFloat)
  .Input("indices", "dtype")
  .Attr("var_name", AttrValue::kString)
  .Attr("var_type", AttrValue::kString)
  .Attr("dtype", AttrValue::kDataType);

DEFINE_INT_OP(XDL_REGISTER_KERNEL(PsSparseApplyFtrlOp, PsSparseApplyFtrlOp<T>)
              .Device("CPU")
              .AttrDataType<T>("dtype"))

} // namespace xdl
