//
//  unwindmacos.hpp
//  tinyinst
//
//  Created by Alexandru-Vlad Niculae on 06.07.21.
//

#ifndef unwindmacos_hpp
#define unwindmacos_hpp

//#include <unordered_map>
#include <vector>

#include <stdio.h>
#include "unwind.h"
#include "tinyinst.h"

class UnwindDataMacOS: public UnwindData {
public:
  UnwindDataMacOS();
  ~UnwindDataMacOS() = default;

  void *addr;
  uint64_t size;
  void *buffer;
  struct unwind_info_section_header *header;

  void AddEncoding(size_t first_original_address,
                   uint32_t encoding,
                   size_t translated_address);

  struct Metadata {
    size_t first_original_address;
    uint32_t encoding;
    size_t min_address;
    size_t max_address;

    Metadata();

    Metadata(size_t first_original_address,
             uint32_t encoding,
             size_t min_address,
             size_t max_address)
    : first_original_address(first_original_address),
      encoding(encoding),
      min_address(min_address),
      max_address(max_address)
    {}

//    bool operator < (const Metadata &other) const {
//      return first_original_address < other.first_original_address;
//    }
//
//    bool operator == (const Metadata &other) const {
//      return first_original_address == other.first_original_address;
//    }
  };

  std::vector<Metadata> metadata_list;
};

class UnwindGeneratorMacOS : public UnwindGenerator {
public:
  UnwindGeneratorMacOS(TinyInst& tinyinst);
  ~UnwindGeneratorMacOS() = default;

  void OnModuleInstrumented(ModuleInfo* module);
  void OnModuleUninstrumented(ModuleInfo* module);

//  size_t MaybeRedirectExecution(ModuleInfo* module, size_t IP) {
//    return IP;
//  }
//
  void OnBasicBlockStart(ModuleInfo* module,
                         size_t original_address,
                         size_t translated_address);
//
  void OnInstruction(ModuleInfo* module,
                     size_t original_address,
                     size_t translated_address);

  void OnBasicBlockEnd(ModuleInfo* module,
                       size_t original_address,
                       size_t translated_address);

//protected:
//  TinyInst& tinyinst_;

private:
  void FirstLevelLookup(ModuleInfo *module, size_t original_address, size_t translated_address);
  void SecondLevelLookup(ModuleInfo *module,
                        size_t original_address,
                        size_t translated_address,
                        struct unwind_info_section_header_index_entry *first_level_entry);
};

#endif /* unwindmacos_hpp */
