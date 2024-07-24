# ESP-IDF Template project for GitHub Codespaces 

This project template can be used to build an ESP-IDF application in GitHub Codespaces, then flash it to the development board and monitor the output right from the web browser. No tools need to be installed locally.

> [!NOTE]
> GitHub Codespaces is free for individuals for a limited time per month. See https://github.com/features/codespaces#pricing for details.

> [!WARNING]
> This repository is a work in progress. If you run into any issue, please report it at https://github.com/espressif/vscode-extension-codespace-test/issues!

## How to Use the Template

1. In https://github.com/espressif/vscode-extension-codespace-test, press the dropdown on the green "Use this template" button on top right, and select "Open in a codespace".

2. The codespace will open and start up. This initial step might take a couple of minutes.

3. To build the example project, press the :wrench: icon on the bottom bar.

   Alternatively, press F1 to show all the commands, then type "idf build". Select "ESP-IDF: Build your project" command. The last used commands are kept at the top, so you don't need to type the name each time.

4. When the build is done, open the commands list again by pressing F1, and look for "ESP-IDF-Web Flash". Select this command.

   You will receive several prompts:

   - Pick the workspace folder: select the only available option (`/workspaces`)

   - Web browser pop up will prompt you to select the serial port. Pick the port of your development board.

   - Select baud rate: pick the highest (921600). Retry with a lower one in case flashing fails.

   Once you see "ESP-IDF Web Flashing done" message, it means that this step is completed.

5. Open the commands list (F1) and select "ESP-IDF-Web Monitor". Select this command.

   When prompted, pick:

   - The port of the development board

   - The baud rate: select 115200

   The monitor should open and you should see "Hello, World" being printed every second.

## Hardware Required

Any ESP32, ESP32-C, ESP32-H, ESP32-S or ESP32-P series development board. Make sure you have serial port drivers installed for this board.

