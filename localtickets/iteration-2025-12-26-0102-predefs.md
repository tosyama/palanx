# Task: Predefined C Macros & Config — 2025-12-26T15:39:23.416Z

ID: IT-2025-12-26-0102

Description:
- Ensure src/c2ast/predefined.h and c2ast include-path handling cover minimal macros and std include search needed to represent <stdio.h> declarations.

Acceptance Criteria:
- predefined.h contains necessary macros used by minimal stdio.h stub.
- c2ast accepts a configurable search path for system headers.
- Tests validate parsing of a stdio.h stub.

Owner: Backend
Status: Closed
ClosedAt: 2025-12-26T15:49:27.092Z
ClosedReason: Predefined macros, configurable search paths, and a c2ast test for stdio.h are present.
Estimate: 0.25d
