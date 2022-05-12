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

#ifndef COVERAGE_H
#define COVERAGE_H

#include <string>
#include <set>
#include <list>
#include <vector>

#define CF_BITMASK 0x0001
#define ZF_BITMASK 0x0040
#define SF_BITMASK 0x0080
#define OF_BITMASK 0x0800

#define CF_BIT(x) ((x&CF_BITMASK)?1:0)
#define ZF_BIT(x) ((x&ZF_BITMASK)?1:0)
#define SF_BIT(x) ((x&SF_BITMASK)?1:0)
#define OF_BIT(x) ((x&OF_BITMASK)?1:0)

enum I2SInstType {
  CMPB,
  CMPL,
  CMPE,
  CMPG,
  CMPA,
  CALL
};

struct I2SRecord {
  I2SInstType type;
  
  bool hit;
  bool ignored;
  int op_length;
  
  size_t bb_address; // for debugging
  size_t bb_offset;
  size_t cmp_offset;
  uint64_t cmp_code;
  size_t instrumentation_offset;
  size_t instrumentation_size;
};

struct I2SData {
  std::vector<uint8_t> op_val[2];
  size_t flags_reg;
  
  I2SRecord *i2s_record;
  
  void PrettyPrint() {
    printf("----I2SRecord----\n");
    printf("type: %d hit: %d\n", i2s_record->type, i2s_record->hit);
    printf("flags: 0x%zx CF: %d ZF: %d SF: %d OF: %d\n", flags_reg,
           CF_BIT(flags_reg), ZF_BIT(flags_reg), SF_BIT(flags_reg), OF_BIT((flags_reg)));
    
    printf("op0: ");
    for (auto &byte: op_val[0]) {
      printf("0x%02hhx ", byte);
    }
    printf("\n");
    
    printf("op1: ");
    for (auto &byte: op_val[1]) {
      printf("0x%02hhx ", byte);
    }
    printf("\n");
    
    printf("----------------\n");
  }
  
  bool BranchPath() {
    switch (i2s_record->type) {
      case CMPB:
        return CF_BIT(flags_reg);
        
      case CMPL:
        return SF_BIT(flags_reg) ^ OF_BIT(flags_reg);
        
      case CMPE:
        return ZF_BIT(flags_reg);
        
      case CMPA:
        return ~CF_BIT(flags_reg) & ~ZF_BIT(flags_reg);
        
      case CMPG:
        return ~(SF_BIT(flags_reg) ^ OF_BIT(flags_reg)) & ~ZF_BIT(flags_reg);
        
      default:
        return false;
    }
  }
};

class ModuleCoverage {
public:
  ModuleCoverage();
  ModuleCoverage(std::string& name, std::set<uint64_t> offsets);

  std::string module_name;
  std::set<uint64_t> offsets;
};

typedef std::list<ModuleCoverage> Coverage;

ModuleCoverage *GetModuleCoverage(Coverage &coverage, std::string &name);

void PrintCoverage(Coverage& coverage);
void WriteCoverage(Coverage& coverage, char *filename);

void MergeCoverage(Coverage &coverage, Coverage &toAdd);
void CoverageIntersection(Coverage &coverage1,
                       Coverage &coverage2,
                       Coverage &result);
// returns coverage2 not present in coverage1
void CoverageDifference(Coverage &coverage1,
                        Coverage &coverage2,
                        Coverage &result);
// returns coverage2 not present in coverage1 and vice versa
void CoverageSymmetricDifference(Coverage &coverage1,
                                Coverage &coverage2,
                                Coverage &result);
bool CoverageContains(Coverage &coverage1, Coverage &coverage2);

void ReadCoverageBinary(Coverage& coverage, char *filename);
void ReadCoverageBinary(Coverage& coverage, FILE *fp);
void WriteCoverageBinary(Coverage& coverage, char *filename);
void WriteCoverageBinary(Coverage& coverage, FILE *fp);

#endif // COVERAGE_H
