// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
// https://developers.google.com/protocol-buffers/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <google/protobuf/compiler/swift/swift_file.h>
#include <google/protobuf/compiler/swift/swift_enum.h>
#include <google/protobuf/compiler/swift/swift_extension.h>
#include <google/protobuf/compiler/swift/swift_message.h>
#include <google/protobuf/compiler/code_generator.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/stubs/stl_util.h>
#include <google/protobuf/stubs/strutil.h>
#include <sstream>

namespace google {
namespace protobuf {

// This is also found in GPBBootstrap.h, and needs to be kept in sync.  It
// is the version check done to ensure generated code works with the current
// runtime being used.
const int32 GOOGLE_PROTOBUF_OBJC_GEN_VERSION = 30000;

namespace compiler {
namespace swift {
FileGenerator::FileGenerator(const FileDescriptor *file)
    : file_(file),
      root_class_name_(FileClassName(file)),
      is_filtered_(true),
      all_extensions_filtered_(true) {
  for (int i = 0; i < file_->enum_type_count(); i++) {
    EnumGenerator *generator = new EnumGenerator(file_->enum_type(i));
    // The enums are exposed via C functions, so they will dead strip if
    // not used.
    is_filtered_ &= false;
    enum_generators_.push_back(generator);
  }
  for (int i = 0; i < file_->message_type_count(); i++) {
    MessageGenerator *generator =
        new MessageGenerator(root_class_name_, file_->message_type(i));
    is_filtered_ &= generator->IsFiltered();
    is_filtered_ &= generator->IsSubContentFiltered();
    message_generators_.push_back(generator);
  }
  for (int i = 0; i < file_->extension_count(); i++) {
    ExtensionGenerator *generator =
        new ExtensionGenerator(root_class_name_, file_->extension(i));
    is_filtered_ &= generator->IsFiltered();
    all_extensions_filtered_ &= generator->IsFiltered();
    extension_generators_.push_back(generator);
  }
  // If there is nothing in the file we filter it.
}

FileGenerator::~FileGenerator() {
  STLDeleteContainerPointers(dependency_generators_.begin(),
                             dependency_generators_.end());
  STLDeleteContainerPointers(enum_generators_.begin(), enum_generators_.end());
  STLDeleteContainerPointers(message_generators_.begin(),
                             message_generators_.end());
  STLDeleteContainerPointers(extension_generators_.begin(),
                             extension_generators_.end());
}

void DetermineDependenciesWorker(set<string> *dependencies,
                                 set<string> *seen_files,
                                 const string &classname,
                                 const FileDescriptor *file) {
  if (seen_files->find(file->name()) != seen_files->end()) {
    // don't infinitely recurse
    return;
  }

  seen_files->insert(file->name());

  for (int i = 0; i < file->dependency_count(); i++) {
    DetermineDependenciesWorker(dependencies, seen_files, classname,
                                file->dependency(i));
  }
  for (int i = 0; i < file->message_type_count(); i++) {
    MessageGenerator(classname, file->message_type(i))
        .DetermineDependencies(dependencies);
  }
}

void FileGenerator::DetermineDependencies(set<string> *dependencies) {
  set<string> seen_files;
  DetermineDependenciesWorker(dependencies, &seen_files, root_class_name_,
                              file_);
}

void FileGenerator::GenerateSource(io::Printer *printer) {
  printer->Print(
      "// Generated by the protocol buffer compiler.  DO NOT EDIT!\n"
      "// source: $filename$\n"
      "\n",
      "filename", file_->name());

  if (IsFiltered()) {
    return;
  }

  printer->Print("#import \"GPBProtocolBuffers_RuntimeSupport.h\"\n\n");

  string header_file = Path() + ".pbobjc.h";

  printer->Print(
      "#import \"$header_file$\"\n"
      "\n"
      "#pragma mark - $root_class_name$\n"
      "\n"
      "@implementation $root_class_name$\n\n",
      "header_file", header_file,
      "root_class_name", root_class_name_);

  bool generated_extensions = false;
  if (file_->extension_count() + file_->message_type_count() +
          file_->dependency_count() >
      0) {
    ostringstream extensions_stringstream;

    if (file_->extension_count() + file_->message_type_count() > 0) {
      io::OstreamOutputStream extensions_outputstream(&extensions_stringstream);
      io::Printer extensions_printer(&extensions_outputstream, '$');
      extensions_printer.Print(
          "static GPBExtensionDescription descriptions[] = {\n");
      extensions_printer.Indent();
      for (vector<ExtensionGenerator *>::iterator iter =
               extension_generators_.begin();
           iter != extension_generators_.end(); ++iter) {
        (*iter)->GenerateStaticVariablesInitialization(
            &extensions_printer, &generated_extensions, true);
      }
      for (vector<MessageGenerator *>::iterator iter =
               message_generators_.begin();
           iter != message_generators_.end(); ++iter) {
        (*iter)->GenerateStaticVariablesInitialization(&extensions_printer,
                                                       &generated_extensions);
      }
      extensions_printer.Outdent();
      extensions_printer.Print("};\n");
      if (generated_extensions) {
        extensions_printer.Print(
            "for (size_t i = 0; i < sizeof(descriptions) / sizeof(descriptions[0]); ++i) {\n"
            "  GPBExtensionField *extension = [[GPBExtensionField alloc] initWithDescription:&descriptions[i]];\n"
            "  [registry addExtension:extension];\n"
            "  [self globallyRegisterExtension:extension];\n"
            "  [extension release];\n"
            "}\n");
      } else {
        extensions_printer.Print("#pragma unused (descriptions)\n");
      }
      const vector<FileGenerator *> &dependency_generators =
          DependencyGenerators();
      if (dependency_generators.size()) {
        for (vector<FileGenerator *>::const_iterator iter =
                 dependency_generators.begin();
             iter != dependency_generators.end(); ++iter) {
          if (!(*iter)->IsFiltered()) {
            extensions_printer.Print(
                "[registry addExtensions:[$dependency$ extensionRegistry]];\n",
                "dependency", (*iter)->RootClassName());
            generated_extensions = true;
          }
        }
      } else if (!generated_extensions) {
        extensions_printer.Print("#pragma unused (registry)\n");
      }
    }

    if (generated_extensions) {
      printer->Print(
          "+ (GPBExtensionRegistry*)extensionRegistry {\n"
          "  // This is called by +initialize so there is no need to worry\n"
          "  // about thread safety and initialization of registry.\n"
          "  static GPBExtensionRegistry* registry = nil;\n"
          "  if (!registry) {\n"
          "    registry = [[GPBExtensionRegistry alloc] init];\n");

      printer->Indent();
      printer->Indent();

      extensions_stringstream.flush();
      printer->Print(extensions_stringstream.str().c_str());
      printer->Outdent();
      printer->Outdent();

      printer->Print(
          "  }\n"
          "  return registry;\n"
          "}\n"
          "\n"
          "+ (void)load {\n"
          "  @autoreleasepool {\n"
          "    [self extensionRegistry]; // Construct extension registry.\n"
          "  }\n"
          "}\n\n");
    }
  }

  printer->Print("@end\n\n");


  string syntax;
  switch (file_->syntax()) {
    case FileDescriptor::SYNTAX_UNKNOWN:
      syntax = "GPBFileSyntaxUnknown";
      break;
    case FileDescriptor::SYNTAX_PROTO2:
      syntax = "GPBFileSyntaxProto2";
      break;
    case FileDescriptor::SYNTAX_PROTO3:
      syntax = "GPBFileSyntaxProto3";
      break;
  }
  printer->Print(
      "static GPBFileDescriptor *$root_class_name$_FileDescriptor(void) {\n"
      "  // This is called by +initialize so there is no need to worry\n"
      "  // about thread safety of the singleton.\n"
      "  static GPBFileDescriptor *descriptor = NULL;\n"
      "  if (!descriptor) {\n"
      "    descriptor = [[GPBFileDescriptor alloc] initWithPackage:@\"$package$\"\n"
      "                                                     syntax:$syntax$];\n"
      "  }\n"
      "  return descriptor;\n"
      "}\n"
      "\n",
      "root_class_name", root_class_name_,
      "package", file_->package(),
      "syntax", syntax);

  for (vector<EnumGenerator *>::iterator iter = enum_generators_.begin();
       iter != enum_generators_.end(); ++iter) {
    (*iter)->GenerateSource(printer);
  }
  for (vector<MessageGenerator *>::iterator iter = message_generators_.begin();
       iter != message_generators_.end(); ++iter) {
    (*iter)->GenerateSource(printer);
  }
}

const string FileGenerator::Path() const { return FilePath(file_); }

const vector<FileGenerator *> &FileGenerator::DependencyGenerators() {
  if (file_->dependency_count() != dependency_generators_.size()) {
    for (int i = 0; i < file_->dependency_count(); i++) {
      FileGenerator *generator = new FileGenerator(file_->dependency(i));
      dependency_generators_.push_back(generator);
    }
  }
  return dependency_generators_;
}
}  // namespace swift
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
