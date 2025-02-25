# CMake generated Testfile for 
# Source directory: /Users/zarniwoop/Source/c/ceed_parser
# Build directory: /Users/zarniwoop/Source/c/ceed_parser/build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(all_tests "/Users/zarniwoop/Source/c/ceed_parser/build/bin/ceed_parser_tests")
set_tests_properties(all_tests PROPERTIES  _BACKTRACE_TRIPLES "/Users/zarniwoop/Source/c/ceed_parser/CMakeLists.txt;358;add_test;/Users/zarniwoop/Source/c/ceed_parser/CMakeLists.txt;0;")
add_test(mnemonic_tests "/Users/zarniwoop/Source/c/ceed_parser/build/bin/ceed_parser_tests" "mnemonic")
set_tests_properties(mnemonic_tests PROPERTIES  _BACKTRACE_TRIPLES "/Users/zarniwoop/Source/c/ceed_parser/CMakeLists.txt;359;add_test;/Users/zarniwoop/Source/c/ceed_parser/CMakeLists.txt;0;")
add_test(wallet_tests "/Users/zarniwoop/Source/c/ceed_parser/build/bin/ceed_parser_tests" "wallet")
set_tests_properties(wallet_tests PROPERTIES  _BACKTRACE_TRIPLES "/Users/zarniwoop/Source/c/ceed_parser/CMakeLists.txt;360;add_test;/Users/zarniwoop/Source/c/ceed_parser/CMakeLists.txt;0;")
add_test(parser_tests "/Users/zarniwoop/Source/c/ceed_parser/build/bin/ceed_parser_tests" "parser")
set_tests_properties(parser_tests PROPERTIES  _BACKTRACE_TRIPLES "/Users/zarniwoop/Source/c/ceed_parser/CMakeLists.txt;361;add_test;/Users/zarniwoop/Source/c/ceed_parser/CMakeLists.txt;0;")
