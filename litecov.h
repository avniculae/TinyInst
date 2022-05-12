/*
Copyright 2020 Google LLC

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

https ://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#ifndef LITECOV_H
#define LITECOV_H

#include <unordered_map>
#include <set>

#include "coverage.h"
#include "tinyinst.h"
#include "instruction.h"

#define COVERAGE_SIZE 0

enum CovType {
  COVTYPE_BB,
  COVTYPE_EDGE
};

struct CmpCoverageRecord {
  bool ignored;
  int width;
  int match_width;
  size_t bb_address; // for debugging
  size_t bb_offset;
  size_t cmp_offset;
  size_t instrumentation_offset;
  size_t instrumentation_size;
  size_t match_width_offset;
};

class ModuleCovData {
public:
  ModuleCovData();
  void ClearInstrumentationData();

  unsigned char *coverage_buffer_remote;

  size_t coverage_buffer_size;
  size_t coverage_buffer_next;

  std::set<uint64_t> collected_coverage;
  std::set<uint64_t> ignore_coverage;
  std::set<uint64_t> saved_coverage;

  // maps offset in the coverage buffer to
  // offset of the basic block / edge code
  std::unordered_map<size_t, uint64_t> buf_to_coverage;

  // maps coverage code (e.g. a bb offset)
  // to offset in the instrumented buffer
  // of the corresponding instrumentation
  std::unordered_map<uint64_t, size_t> coverage_to_inst;

  bool has_remote_coverage;

  void ClearCmpCoverageData();
  std::unordered_map<size_t, CmpCoverageRecord*> buf_to_cmp;
  std::unordered_map<uint64_t, CmpCoverageRecord*> coverage_to_cmp;
  
  void ClearI2SRecords();
  uint8_t *i2s_buffer_remote;
  size_t i2s_buffer_next;
  
  std::vector<I2SData> collected_i2s_data;
  std::unordered_map<size_t, I2SRecord*> buf_to_i2s;
  std::unordered_map<uint64_t, I2SRecord*> coverage_to_i2s;
};

class LiteCov : public TinyInst {
public:
  virtual void Init(int argc, char **argv) override;

  void GetCoverage(Coverage &coverage, bool clear_coverage);
  void ClearCoverage();
  void EnableFullCoverage();
  void DisableFullCoverage();

  // note: this does not affect already collected coverage
  void IgnoreCoverage(Coverage &coverage);

  bool HasNewCoverage();
  
  void EnableInputToState();
  void DisableInputToState();
  
  std::vector<I2SData> GetI2SData(bool clear_i2s);
  void ClearI2SData();
  std::unordered_map<uint64_t, uint64_t> tree_father;

protected:
  virtual void OnModuleInstrumented(ModuleInfo *module) override;
  virtual void OnModuleUninstrumented(ModuleInfo *module) override;

  virtual void OnProcessExit() override;

  virtual void OnModuleEntered(ModuleInfo *module, size_t entry_address) override;
  virtual bool OnException(Exception *exception_record) override;

  virtual void InstrumentBasicBlock(ModuleInfo *module, size_t bb_address) override;
  virtual void InstrumentEdge(ModuleInfo *previous_module,
                              ModuleInfo *next_module,
                              size_t previous_address,
                              size_t next_address) override;
  virtual InstructionResult InstrumentInstruction(ModuleInfo *module,
                                                  Instruction &inst,
                                                  size_t bb_address,
                                                  size_t instruction_address,
                                                  bool before = true) override;
  
  virtual InstructionResult InstrumentInstructionCmpCoverage(ModuleInfo *module,
                                                             Instruction &inst,
                                                             size_t bb_address,
                                                             size_t instruction_address);
  
  virtual InstructionResult InstrumentInstructionI2S(ModuleInfo *module,
                                                     Instruction &inst,
                                                     size_t bb_address,
                                                     size_t instruction_address);

  void EmitCoverageInstrumentation(ModuleInfo *module, uint64_t coverage_code);
  void EmitCoverageInstrumentation(ModuleInfo *module,
                                    size_t bit_address,
                                    size_t mov_address);
  void ClearCoverageInstrumentation(ModuleInfo *module, uint64_t coverage_code);

  void NopCovInstructions(ModuleInfo *module, size_t code_offset);
  void NopCmpCovInstructions(ModuleInfo *module,
                             CmpCoverageRecord &cmp_record,
                             int matched_width);
  
  void NopI2SInstructions(ModuleInfo *module,
                          I2SRecord &i2s_record);
//  void TemporarlyNopI2SInstructions(ModuleInfo *module,
//                                    size_t instrumentation_offset,
//                                    size_t instrumentation_size);
//  void UnnopTemporarlyNoppedI2SInstructions(ModuleInfo *module,
//                                            size_t instrumentation_offset);


  // compute a unique code for a basic block
  // this is just an offset into the module
  uint64_t GetBBCode(ModuleInfo *module, size_t bb_address);

  // compute a unique code for a basic block
  // this has address1 offset in lower 32 bits and
  // address2 offset in higher 32 bits
  uint64_t GetEdgeCode(ModuleInfo *module, size_t edge_address1, size_t edge_address2);

  ModuleCovData *GetDataByRemoteAddress(size_t address);
  void HandleBufferWriteException(ModuleCovData *data);

  void ClearCoverage(ModuleCovData *data);
  void ClearRemoteBuffer(ModuleCovData *data);

  void CollectCoverage(ModuleCovData *data);
  void CollectCoverage();
  
  void EnableFullCoverage(ModuleCovData *data);
  void DisableFullCoverage(ModuleCovData *data);
  
  void ClearI2SData(ModuleCovData *data);
  void CollectI2SData(ModuleCovData *data);
  void CollectI2SData();
  void ClearI2SRemoteBuffer(ModuleCovData *data);

  uint64_t GetCmpCode(size_t bb_offset, size_t cmp_offset, int bits_match);
  bool IsCmpCoverageCode(uint64_t code);
  void ClearCmpCoverageInstrumentation(ModuleInfo *module, uint64_t coverage_code);
  void ClearI2SInstrumentation(ModuleInfo *module, uint64_t coverage_code);
  void CollectCmpCoverage(ModuleCovData *data, size_t buffer_offset, char buffer_value);
  bool ShouldInstrumentSub(ModuleInfo *module,
                           Instruction& cmp_instr,
                           size_t instruction_address);
  
  xed_iclass_enum_t NextCondIclass(ModuleInfo *module, Instruction &cmp_instr,
                                   size_t instruction_address);
  
  I2SInstType GetI2SInstType(xed_iclass_enum_t next_cond_iclass);
  
private:
  CovType coverage_type;
  bool compare_coverage;
  bool original_compare_coverage;
  
  bool input_to_state;
  int I2S_BUFFER_SIZE;
  bool i2s_mutator_enabled;
};

#endif // LITECOV_H
