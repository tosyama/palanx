This is a C++-based repository for the Palan programming language.

# Repository Structure
- `src/`: Source code for the Palan compiler tools.
- `src/build-mgr/`: Source code for the build manager command-line tool.
- `src/c2ast/`: Source code for the C-to-AST translator command-line tool.
- `src/gen-ast/`: Source code for the AST generator command-line tool.
- `src/semantic-anlyzr/`: Source code for the semantic analyzer command-line tool.
- `lib/`: Third-party libraries.
- `test/`: Unit and integration tests.
- `doc/`: Documentation files.
- `build/`: Build output directory (created after building).
- `localtickets/`: Local tickets for coding agents. Save tasks and their status here in Markdown format.

# Documentation
- `README.md`: Overview and build instructions for the repository.
- `doc/SpecAndDesign.md`: Specification and basic design of the Palan programming language.
- `doc/ASTSpec.md`: Specification of the Abstract Syntax Tree (AST) structure.

# Testing policy
- Automated tests are located in the `test/` directory.
- Tests are written using the Google Test framework.
- It is recommended to created test by running command-line tools with corresponding test input files and comparing the output with expected results.
- Target test coverage is at least 95% for all modules.

