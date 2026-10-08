Remove-Item ./CompiledResultExport.* -Force
Compress-Archive -path ./build/HelmBoyStandalone_artefacts\Release\helmBoy.exe -destinationPath ./compiledResultExport.zip
Compress-Archive -Path ./build\HelmBoyPlugin_artefacts\Release\VST3\helmBoy.vst3 -DestinationPath ./compiledResultExport.zip -Update
Compress-Archive -Path ./build\HelmBoyPlugin_artefacts\Release\LV2\helmBoy.lv2 -DestinationPath ./compiledResultExport.zip -Update
Copy-Item -Path ./compiledResultExport.zip ./CompiledResultExport.txt -Force