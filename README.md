# Voice Over
Module to support working with the VoiceOver (AI generated npc voices) addon

You can download the audio files from here https://www.curseforge.com/members/darkneo/projects

# Available Cores
Classic

# How to install
1. Follow the instructions in https://github.com/davidonete/cmangos-modules?tab=readme-ov-file#how-to-install
2. Enable the `BUILD_MODULE_VOICEOVER` flag in cmake and run cmake. The module should be installed in `src/modules/voiceover`.
3. Copy the configuration file from `src/modules/voiceover/src/voiceover.conf.dist.in` and place it where your mangosd executable is. Also rename it to `voiceover.conf`.
4. Remember to edit the config file and modify the options you want to use.
5. Install the addon to your client located in `src/modules/voiceover/addons/1.12/AI_VoiceOver`

# How to uninstall
To remove VoiceOver from your server you have to remove it from the server and client:
1. Remove the `BUILD_MODULE_VOICEOVER` flag from your cmake configuration and recompile the game
2. Delete the config file or disable the module in `voiceover.conf`
3. Delete or disable the client addon `AI_VoiceOver`