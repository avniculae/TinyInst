//
//  unwindgenerator_macos.cpp
//  tinyinst
//
//  Created by Alexandru-Vlad Niculae on 06.07.21.
//

#include "unwindmacos.hpp"

#include <mach-o/dyld.h>
#include <mach-o/dyld_images.h>
#include <mach-o/nlist.h>
#include <mach-o/compact_unwind_encoding.h>

UnwindDataMacOS::UnwindDataMacOS() {
  addr = NULL;
  size = 0;
  buffer = NULL;
  header = NULL;
}

void UnwindDataMacOS::AddEncoding(size_t first_original_address, uint32_t encoding, size_t translated_address) {
  if (metadata_list.empty() || metadata_list.back().first_original_address != first_original_address) {
    metadata_list.push_back(Metadata(first_original_address, encoding, translated_address, translated_address));
  }
  else {
    metadata_list.back().max_address = translated_address;
  }
}

UnwindGeneratorMacOS::UnwindGeneratorMacOS(TinyInst& tinyinst) : UnwindGenerator(tinyinst) {

}


void UnwindGeneratorMacOS::OnModuleInstrumented(ModuleInfo* module) {
  printf("Inside Unwind Generator, module %s 0x%llx\n", module->module_name.c_str(), module->module_header);

  section_64 unwind_section = tinyinst_.GetSection(module->module_header, "__TEXT", "__unwind_info");

  module->unwind_data = new UnwindDataMacOS();

  module->unwind_data->addr = (void*)((uint64_t)module->module_header + unwind_section.offset);
  module->unwind_data->size = unwind_section.size;

  module->unwind_data->buffer = (void*)malloc(unwind_section.size);
  tinyinst_.RemoteRead(module->unwind_data->addr,
                       module->unwind_data->buffer,
                       module->unwind_data->size);

  module->unwind_data->header = (struct unwind_info_section_header *)module->unwind_data->buffer;
}

void UnwindGeneratorMacOS::OnBasicBlockStart(ModuleInfo* module,
                                             size_t original_address,
                                             size_t translated_address) {
//  printf("\n-----------------------\n");
//  printf("OnBasicBlockStart original addr: 0x%llx translated addr: 0x%llx\n", original_address, translated_address);
//  printf("metadata vector before: \n");
//  for (auto &it: module->unwind_data->metadata_list) printf("first: 0x%llx encoding: %u min: 0x%llx max: 0x%llx\n", it.first_original_address, it.encoding, it.min_address, it.max_address);
  FirstLevelLookup(module, original_address, translated_address);
//  printf("metadata vector after: \n");
//  for (auto &it: module->unwind_data->metadata_list) printf("first: 0x%llx encoding: %u min: 0x%llx max: 0x%llx\n", it.first_original_address, it.encoding, it.min_address, it.max_address);
//  printf("-----------------------\n");
}

void UnwindGeneratorMacOS::OnInstruction(ModuleInfo* module,
                                         size_t original_address,
                                         size_t translated_address) {
//  printf("\n-----------------------\n");
//  printf("OnInstruction original addr: 0x%llx translated addr: 0x%llx\n", original_address, translated_address);
//  printf("metadata vector before: \n");
//  for (auto &it: module->unwind_data->metadata_list) printf("first: 0x%llx encoding: %u min: 0x%llx max: 0x%llx\n", it.first_original_address, it.encoding, it.min_address, it.max_address);
  FirstLevelLookup(module, original_address, translated_address);
//  printf("metadata vector after: \n");
//  for (auto &it: module->unwind_data->metadata_list) printf("first: 0x%llx encoding: %u min: 0x%llx max: 0x%llx\n", it.first_original_address, it.encoding, it.min_address, it.max_address);
//  printf("-----------------------\n");
}

void UnwindGeneratorMacOS::FirstLevelLookup(ModuleInfo *module, size_t original_address, size_t translated_address) {
  size_t original_offset = original_address - (uint64_t)module->module_header;

  size_t curr_first_level_entry_addr = (size_t)module->unwind_data->buffer
                        + module->unwind_data->header->indexSectionOffset;

  for (int entry_cnt = 0; entry_cnt < module->unwind_data->header->indexCount; ++entry_cnt) {
    struct unwind_info_section_header_index_entry *curr_first_level_entry =
      (struct unwind_info_section_header_index_entry *)curr_first_level_entry_addr;

    if (curr_first_level_entry->functionOffset <= original_offset) {
      if (entry_cnt + 1 == module->unwind_data->header->indexCount) {
        SecondLevelLookup(module, original_address, translated_address, curr_first_level_entry);
        return;
      }

      struct unwind_info_section_header_index_entry *next_first_level_entry =
        (struct unwind_info_section_header_index_entry *)(curr_first_level_entry_addr + sizeof(struct unwind_info_section_header_index_entry));

      if (original_offset < next_first_level_entry->functionOffset) {
        SecondLevelLookup(module, original_address, translated_address, curr_first_level_entry);
        return;
      }
    }

//    printf("index section 0x%llx: functionOffset 0x%llx header_addr+functionOffset 0x%llx, "
//           "secondLevelPagesSectionOffset 0x%llx, lsdaIndexArraySectionOffset 0x%llx\n",
//           cnt, curr_index_entry->functionOffset, (uint64_t)module->module_header + curr_index_entry->functionOffset,
//           curr_index_entry->secondLevelPagesSectionOffset,
//           curr_index_entry->lsdaIndexArraySectionOffset);

    curr_first_level_entry_addr += sizeof(struct unwind_info_section_header_index_entry);
  }
}


void UnwindGeneratorMacOS::SecondLevelLookup(ModuleInfo *module,
                                            size_t original_address,
                                            size_t translated_address,
                                            struct unwind_info_section_header_index_entry *first_level_entry) {
  size_t original_offset = original_address - (uint64_t)module->module_header;

  size_t second_level_page_addr = (size_t)module->unwind_data->buffer
                                  + first_level_entry->secondLevelPagesSectionOffset;

  if (*(uint32_t*)second_level_page_addr == UNWIND_SECOND_LEVEL_COMPRESSED) {
    struct unwind_info_compressed_second_level_page_header *second_level_header =
      (struct unwind_info_compressed_second_level_page_header *)second_level_page_addr;

    size_t curr_second_level_entry_addr = second_level_page_addr + second_level_header->entryPageOffset;
    for (int entry_cnt = 0; entry_cnt < second_level_header->entryCount; ++entry_cnt) {
      uint32_t curr_entry = *(uint32_t*)curr_second_level_entry_addr;
      uint32_t curr_entry_encoding_index = UNWIND_INFO_COMPRESSED_ENTRY_ENCODING_INDEX(curr_entry);
      uint32_t curr_entry_func_offset = UNWIND_INFO_COMPRESSED_ENTRY_FUNC_OFFSET(curr_entry);

      bool entry_match = false;
      uint32_t encoding;
      if ((uint64_t)module->module_header + first_level_entry->functionOffset + curr_entry_func_offset <= original_address) {
        if (entry_cnt + 1 == second_level_header->entryCount) {
          entry_match = true;
        }

        uint32_t next_entry = *(uint32_t*)(curr_second_level_entry_addr + sizeof(uint32_t));
        uint32_t next_entry_encoding_index = UNWIND_INFO_COMPRESSED_ENTRY_ENCODING_INDEX(next_entry);
        uint32_t next_entry_func_offset = UNWIND_INFO_COMPRESSED_ENTRY_FUNC_OFFSET(next_entry);
        if (original_address < (uint64_t)module->module_header + first_level_entry->functionOffset + next_entry_func_offset) {
          entry_match = true;
        }
      }

      if (entry_match) {
        uint32_t encoding;
        if (curr_entry_encoding_index < module->unwind_data->header->commonEncodingsArrayCount) {
          encoding = *(uint32_t*)((size_t)module->unwind_data->buffer
                                  + module->unwind_data->header->commonEncodingsArraySectionOffset
                                  + curr_entry_encoding_index * sizeof(uint32_t));
        } else {
          encoding = *(uint32_t*)((size_t)module->unwind_data->buffer
                                  + second_level_header->encodingsPageOffset
                                  + (curr_entry_encoding_index - module->unwind_data->header->commonEncodingsArrayCount) * sizeof(uint32_t));
        }


        module->unwind_data->AddEncoding((uint64_t)module->module_header + first_level_entry->functionOffset + curr_entry_func_offset,
                                        encoding,
                                        translated_address);
        return;
      }

      curr_second_level_entry_addr += sizeof(uint32_t);
    }
  } else if (*(uint32_t*)second_level_page_addr == UNWIND_SECOND_LEVEL_REGULAR) {
    struct unwind_info_regular_second_level_page_header *second_level_header =
      (struct unwind_info_regular_second_level_page_header *)second_level_page_addr;

    size_t curr_second_level_entry_addr = second_level_page_addr + second_level_header->entryPageOffset;
    for (int entry_cnt = 0; entry_cnt < second_level_header->entryCount; ++entry_cnt) {
      struct unwind_info_regular_second_level_entry *curr_entry =
        (struct unwind_info_regular_second_level_entry *)curr_second_level_entry_addr;

      bool entry_match = false;
      uint32_t encoding;
      if ((uint64_t)module->module_header + curr_entry->functionOffset <= original_address) {
        if (entry_cnt + 1 == second_level_header->entryCount) {
          entry_match = true;
        }

        struct unwind_info_regular_second_level_entry *next_entry =
          (struct unwind_info_regular_second_level_entry *)(curr_second_level_entry_addr
                                                            + sizeof(struct unwind_info_regular_second_level_entry));
        if (original_address < (uint64_t)module->module_header + next_entry->functionOffset) {
          entry_match = true;
        }
      }

      if (entry_match) {
        module->unwind_data->AddEncoding((uint64_t)module->module_header + curr_entry->functionOffset,
                                        curr_entry->encoding,
                                        translated_address);
        return;
      }


      curr_second_level_entry_addr += sizeof(struct unwind_info_regular_second_level_entry);
    }
  }

}

void UnwindGeneratorMacOS::OnModuleUninstrumented(ModuleInfo *module) {
//  while (1);
}

void UnwindGeneratorMacOS::OnBasicBlockEnd(ModuleInfo* module,
                                           size_t original_address,
                                           size_t translated_address) {

}
