# MemoriaDB
TUM C++ Course project.  in-memory database that supports a simplified SQL syntax for creating tables, inserting and modifying data and running queries. 

TODO:
1. Setup dependencies
2. Setup ci/cd on github actions
   3. Valgrind
   4. GoogleTest/Gmock
   5. GCov
   6. Linters
   7. Write tests


## Clang-format

Check clang formating:
```bash
 git ls-files '*.c' '*.cc' '*.cpp' '*.cxx' '*.h' '*.hh' '*.hpp' \
  | xargs clang-format -i --dry-run -style=file 
```

Tranform files to clang format:
```bash
 git ls-files '*.c' '*.cc' '*.cpp' '*.cxx' '*.h' '*.hh' '*.hpp' \
  | xargs clang-format -i -style=file 
```

