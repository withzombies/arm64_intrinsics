#define _CRT_SECURE_NO_WARNINGS

#include "binaryninjaapi.h"
#include "lowlevelilinstruction.h"
#include <cinttypes>
#include <string>
#include <vector>

using namespace BinaryNinja;

// autogenerated static map of key -> msr name
#include "msr.h"

namespace A64 {
// Intrinsic defines
constexpr uint32_t ISB = 0x4141;
constexpr uint32_t WFI = 0x4142;
constexpr uint32_t WFE = 0x4143;
constexpr uint32_t MSR_IMM = 0x4144;
constexpr uint32_t MSR_REG = 0x4145;
constexpr uint32_t MRS = 0x4146;

// Base for our MSRs
constexpr uint32_t MSR_BASE = 0x4200;
}; // namespace A64

class arm64Intrinsics : public ArchitectureHook {
private:
  static constexpr uint8_t SrOp0(uint32_t ins) {
    return ((ins >> 19) & 0b1) + 2;
  }

  static constexpr uint8_t SrOp1(uint32_t ins) { return (ins >> 16) & 0b111; }

  static constexpr uint8_t SrOp2(uint32_t ins) { return (ins >> 5) & 0b111; }

  static constexpr uint8_t SrXt(uint32_t ins) { return ins & 0b11111; }

  static constexpr uint8_t SrCm(uint32_t ins) { return (ins >> 8) & 0b1111; }

  static constexpr uint8_t SrCn(uint32_t ins) { return (ins >> 12) & 0b1111; }

  static constexpr uint32_t StatusReg(uint8_t o0, uint8_t o1, uint8_t Cn,
                                      uint8_t Cm, uint8_t o2) {
    return o0 << 14 | o1 << 11 | Cn << 7 | Cm << 3 | o2;
  }

  // If we add all the MSRs to the msr map, it adds 15k registers. This map is
  // checked very frequently during startup and adds considerable slowdown to
  // launch times. As a result, we only only add the named status regsiters,
  // however this means its possible we have an MSR intrinsic with an opcode
  // that references a register we haven't added. As a result, we test when
  // decoding that the Intrinsic is referencing a MSR we've added.
  static bool ValidStatusReg(uint8_t o0, uint8_t o1, uint8_t Cn, uint8_t Cm,
                             uint8_t o2) {
    const uint32_t key = StatusReg(o0, o1, Cn, Cm, o2);

    return msr.find(key) != msr.end();
  }

  static const char *StatusRegName(uint32_t key) { return msr[key]; }

  // we need a shotgun arm64 decoder
  static constexpr uint32_t DecodeIntrinsic(const uint8_t *const data,
                                            const size_t sz) {
    if (sz < 4)
      return 0;

    const uint32_t candidate = *(uint32_t *)data;

    const uint32_t isb = 0b11010101000000110011000011011111;
    const uint32_t isb_mask = 0b11111111111111111111000011111111;
    if ((candidate & isb_mask) == isb)
      return A64::ISB;

    const uint32_t wfi = 0b11010101000000110010000001111111;
    if (candidate == wfi)
      return A64::WFI;

    const uint32_t wfe = 0b11010101000000110010000001011111;
    if (candidate == wfe)
      return A64::WFE;

    if (candidate >> 20 == 0b110101010001)
      if (ValidStatusReg(SrOp0(candidate), SrOp1(candidate), SrCn(candidate),
                         SrCm(candidate), SrOp2(candidate)))
        return A64::MSR_REG;

    const uint32_t msr_imm = 0b11010101000000000100000000011111;
    const uint32_t msr_imm_mask = 0b11010101000000000100000000011111;
    if ((candidate & msr_imm_mask) == msr_imm) {
      if (ValidStatusReg(SrOp0(candidate), SrOp1(candidate), SrCn(candidate),
                         SrCm(candidate), SrOp2(candidate)))
        return A64::MSR_IMM;
    }

    if (candidate >> 20 == 0b110101010011)
      if (ValidStatusReg(SrOp0(candidate), SrOp1(candidate), SrCn(candidate),
                         SrCm(candidate), SrOp2(candidate)))
        return A64::MRS;

    return 0;
  }

public:
  explicit arm64Intrinsics(Architecture *arm64) : ArchitectureHook(arm64) {}

  /*
   * First we need to add all the status registers as registers to the
   * architecture so that our intrinsics have a src (mrs) and a dst (msr)
   */
  std::vector<uint32_t> GetAllRegisters() override {
    auto regs = ArchitectureHook::GetAllRegisters();

    for (auto const &ii : msr) {
      regs.push_back(A64::MSR_BASE + ii.first);
    }

    return regs;
  }

  std::vector<uint32_t> GetSystemRegisters() override {
    auto regs = ArchitectureHook::GetSystemRegisters();

    for (auto const &ii : msr) {
      regs.push_back(A64::MSR_BASE + ii.first);
    }

    return regs;
  }

  std::string GetRegisterName(uint32_t reg) override {
    uint32_t key = reg - A64::MSR_BASE;

    if (msr.find(key) != msr.end()) {
      return StatusRegName(key);
    }

    return ArchitectureHook::GetRegisterName(reg);
  }

  BNRegisterInfo GetRegisterInfo(uint32_t reg) override {
    uint32_t key = reg - A64::MSR_BASE;

    if (msr.find(key) != msr.end()) {
      BNRegisterInfo res = {};

      res.fullWidthRegister = reg;
      res.offset = 0;
      res.size = 4;
      res.extend = NoExtend;

      return res;
    }

    return ArchitectureHook::GetRegisterInfo(reg);
  }

  std::vector<uint32_t> GetFullWidthRegisters() override {
    auto regs = ArchitectureHook::GetFullWidthRegisters();

    for (auto const &ii : msr) {
      regs.push_back(A64::MSR_BASE + ii.first);
    }

    return regs;
  }

  /*
   * Now that we've got our regs added, we can add our intrinsics
   */

  std::string GetIntrinsicName(uint32_t intrinsic) override {
    switch (intrinsic) {
    case A64::ISB:
      return "__isb";
    case A64::WFI:
      return "__wfi";
    case A64::WFE:
      return "__wfe";
    case A64::MSR_IMM:
    case A64::MSR_REG:
      return "_WriteStatusReg";
    case A64::MRS:
      return "_ReadStatusReg";
    default:
      break;
    }

    return ArchitectureHook::GetIntrinsicName(intrinsic);
  }

  std::vector<uint32_t> GetAllIntrinsics() override {
    auto intrins = ArchitectureHook::GetAllIntrinsics();

    intrins.push_back(A64::ISB);
    intrins.push_back(A64::WFE);
    intrins.push_back(A64::WFI);
    intrins.push_back(A64::MSR_IMM);
    intrins.push_back(A64::MSR_REG);
    intrins.push_back(A64::MRS);

    return intrins;
  }

  /*
   * Now we need to define the inputs and outputs to our intrinsics. Since
   * we're mostly worried about MSR/MRS right now, we're going to define them
   * as:
   * reg = msr(x0)
   * x0 = mrs(reg)
   *
   * We need to do this because binja doesn't have a nice way for us to express
   * intrinsics as msr(reg, x0) easily in the IL's.
   */

  std::vector<NameAndType> GetIntrinsicInputs(uint32_t intrinsic) override {
    switch (intrinsic) {
    case A64::ISB:
      return {};
    case A64::WFI:
      return {};
    case A64::WFE:
      return {};
    case A64::MSR_IMM:
      return {NameAndType(Type::IntegerType(4, false))};
    case A64::MSR_REG:
      return {NameAndType(Type::IntegerType(8, false))};
    case A64::MRS:
      return {NameAndType(Type::IntegerType(4, false))};
    default:
      break;
    }

    return ArchitectureHook::GetIntrinsicInputs(intrinsic);
  }

  std::vector<Confidence<Ref<Type>>>
  GetIntrinsicOutputs(uint32_t intrinsic) override {
    switch (intrinsic) {
    case A64::ISB:
      return {};
    case A64::WFI:
      return {};
    case A64::WFE:
      return {};
    case A64::MSR_IMM:
    case A64::MSR_REG:
      return {Type::IntegerType(4, false)};
    case A64::MRS:
      return {Type::IntegerType(8, false)};
    default:
      break;
    }

    return ArchitectureHook::GetIntrinsicOutputs(intrinsic);
  }

  bool GetInstructionLowLevelIL(const uint8_t *data, uint64_t addr, size_t &len,
                                LowLevelILFunction &il) override {
    switch (DecodeIntrinsic(data, len)) {
    case A64::ISB:
      il.AddInstruction(il.Intrinsic({}, A64::ISB, vector<ExprId>{}));
      break;
    case A64::WFI:
      il.AddInstruction(il.Intrinsic({}, A64::WFI, vector<ExprId>{}));
      break;
    case A64::WFE:
      il.AddInstruction(il.Intrinsic({}, A64::WFE, vector<ExprId>{}));
      break;
    case A64::MSR_REG: {
      const uint32_t ins = *reinterpret_cast<const uint32_t *>(data);

      il.AddInstruction(il.Intrinsic(
          {
              RegisterOrFlag::Register(
                  A64::MSR_BASE + StatusReg(SrOp0(ins), SrOp1(ins), SrCn(ins),
                                            SrCm(ins), SrOp2(ins))),
          },
          A64::MSR_REG,
          // 34 is the offset into the 64b GPRs from GetAllRegisters
          {il.Register(8, 34 + SrXt(ins))}));
      break;
    }
    case A64::MRS: {
      const uint32_t ins = *reinterpret_cast<const uint32_t *>(data);

      il.AddInstruction(il.Intrinsic(
          // 34 is the offset into the 64b GPRs from GetAllRegisters
          {RegisterOrFlag::Register(34 + SrXt(ins))}, A64::MRS,
          {
              il.Register(4, A64::MSR_BASE + StatusReg(SrOp0(ins), SrOp1(ins),
                                                       SrCn(ins), SrCm(ins),
                                                       SrOp2(ins))),
          }));
      break;
    }
      // intrinsic not found, punt to the arch plugin
    case 0:
    default:
      return ArchitectureHook::GetInstructionLowLevelIL(data, addr, len, il);
    }

    len = 4; // all instructions are 4b
    return true;
  }
};

extern "C" {
BINARYNINJAPLUGIN void CorePluginDependencies() {
  // Make sure we load after the original arm64 plugin loads
  AddRequiredPluginDependency("arch_arm64");
}

BINARYNINJAPLUGIN bool CorePluginInit() {
  // binja uses both arm64 and aarch64, so we just pick one and use it
  Architecture *arm64_intrin =
      new arm64Intrinsics(Architecture::GetByName("aarch64"));
  Architecture::Register(arm64_intrin);
  return true;
}
}
