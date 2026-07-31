import os
import sys

def main():
    print("Applying Windows compatibility patches to Test-Suite...")
    
    # 1. Patch CMakeLists.txt
    cmake_path = 'Test-Suite/CMakeLists.txt'
    if os.path.exists(cmake_path):
        with open(cmake_path, 'r', encoding='utf-8') as f:
            content = f.read()
        
        # Prepend Python3 package find
        if 'find_package(Python3' not in content:
            prefix = "find_package(Python3 COMPONENTS Interpreter REQUIRED)\nset(PYTHON_RUNNER ${Python3_EXECUTABLE})\n\n"
            content = prefix + content
            
        # Replace difftest.py calls to prepend PYTHON_RUNNER
        content = content.replace('COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/diff_tests/difftest.py', 
                                  'COMMAND ${PYTHON_RUNNER} ${CMAKE_CURRENT_SOURCE_DIR}/diff_tests/difftest.py')
                                  
        # Fix cfl_tests duplicate names
        old_cfl = """  foreach(filename ${cfl_files})
  add_test(
    NAME cfl_tests/${filename}POCR"""
        
        new_cfl = """  foreach(filename ${cfl_files})
    if (${filename} MATCHES ".pre.bc" OR ${filename} MATCHES ".pre.svf.bc" OR ${filename} MATCHES ".svf.bc")
      continue()
    endif()
    add_test(
      NAME cfl_tests/${filename}POCR"""
          
        content = content.replace(old_cfl, new_cfl)
        
        with open(cmake_path, 'w', encoding='utf-8') as f:
            f.write(content)
        print("  - Test-Suite/CMakeLists.txt patched successfully.")
    else:
        print("  - Warning: Test-Suite/CMakeLists.txt not found.")

    # 2. Patch difftest.py
    difftest_path = 'Test-Suite/diff_tests/difftest.py'
    if os.path.exists(difftest_path):
        with open(difftest_path, 'r', encoding='utf-8') as f:
            content = f.read()
            
        old_cmds = """    cmd1 = f'./{cmd1} {path1}'
    cmd2 = f'./{cmd2} {path2}'"""
        
        new_cmds = """    import sys
    if sys.platform == 'win32':
        cmd1 = f'.\\\\{cmd1} {path1}'
        cmd2 = f'.\\\\{cmd2} {path2}'
    else:
        cmd1 = f'./{cmd1} {path1}'
        cmd2 = f'./{cmd2} {path2}'"""
            
        content = content.replace(old_cmds, new_cmds)
        
        with open(difftest_path, 'w', encoding='utf-8') as f:
            f.write(content)
        print("  - Test-Suite/diff_tests/difftest.py patched successfully.")
    else:
        print("  - Warning: Test-Suite/diff_tests/difftest.py not found.")

    print("Patches completed.")

if __name__ == '__main__':
    main()
