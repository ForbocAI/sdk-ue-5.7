# ForbocAI SDK Test Project

This is the standalone test/host project for the ForbocAI UE SDK. It is used by SDK contributors to drive the plugin during development, without requiring a separate game checkout.

## Structure and Discovery

The project uses `AdditionalPluginDirectories: [".."]` in its `.uproject` file to dynamically discover and load the `ForbocAI_SDK` plugin from the repository root. This allows the plugin to be developed in a canonical flat layout while still being testable.

## Running Tests

To run the test scenarios, including the parity verifier, contract harness, and automation tests:

1. Build the editor project (`ForbocAI_SDK_Editor`).
2. Run the parity verifier from the repo root:
   ```bash
   bash scripts/verify-ue-parity.sh
   ```
3. Run the automation tests (RunGame entry) via the Unreal Engine session frontend or command line testing tools.
