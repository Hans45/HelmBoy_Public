$destinationPath = $psscriptRoot + "\..\..\HelmBoy_Public\"
$sourcePath = $psscriptRoot + "\..\"
write-host "Copying files from $sourcePath to $destinationPath"
if (Test-Path $destinationPath) {

    Write-Host "Cleaning existing HelmBoyPublic directory..."
    remove-item -path "$destinationPath/binaries" -recurse -force
    remove-item -Path "$destinationPath/concurrentqueue" -Recurse -Force
    remove-item -Path "$destinationPath/docs" -Recurse -Force
    remove-item -Path "$destinationPath/externals" -Recurse -Force
    remove-item -Path "$destinationPath/fonts" -Recurse -Force
    remove-item -Path "$destinationPath/images" -Recurse -Force
    remove-item -Path "$destinationPath/mopo" -Recurse -Force
    remove-item -Path "$destinationPath/patches" -Recurse -Force
    remove-item -Path "$destinationPath/src" -Recurse -Force
    remove-item -Path "$destinationPath/utilities" -Recurse -Force
    Get-childitem -Path $destinationPath -file | Remove-Item -Force

    Write-Host "Creating new HelmBoyPublic directory structure..."
    new-item -path "$destinationPath/binaries" -itemtype directory
    new-item -path "$destinationPath/concurrentqueue" -itemtype directory
    new-item -path "$destinationPath/docs" -itemtype directory
    new-item -path "$destinationPath/externals" -itemtype directory
    new-item -path "$destinationPath/fonts" -itemtype directory
    new-item -path "$destinationPath/images" -itemtype directory
    new-item -path "$destinationPath/mopo" -itemtype directory
    new-item -path "$destinationPath/patches" -itemtype directory
    new-item -path "$destinationPath/src" -itemtype directory
    new-item -path "$destinationPath/utilities" -itemtype directory

    write-host "Copying files to HelmBoyPublic directory..."
    Copy-Item -Path "$sourcePath/Generated Binaries" -Destination "$destinationPath/binaries" -Recurse
    get-childitem -Path "$sourcePath/installer" | sort-object CreationTime | select-object -last 1 | Copy-Item -Destination "$destinationPath/binaries"
    Copy-Item -Path "$sourcePath/docs" -Destination "$destinationPath/docs" -Recurse
    Copy-Item -Path "$sourcePath/externals/ASIOSDK" -Destination "$destinationPath/externals/ASIOSDK" -Recurse
    Copy-Item -Path "$sourcePath/fonts" -Destination "$destinationPath/fonts" -Recurse
    Copy-Item -Path "$sourcePath/images" -Destination "$destinationPath/images" -Recurse
    Copy-Item -Path "$sourcePath/mopo" -Destination "$destinationPath/mopo" -Recurse
    Copy-Item -Path "$sourcePath/patches" -Destination "$destinationPath/patches" -Recurse
    Copy-Item -Path "$sourcePath/src" -Destination "$destinationPath/src" -Recurse
    Copy-Item -Path "$sourcePath/utilities" -Destination "$destinationPath/utilities" -Recurse
    get-childitem -Path "$sourcepath" -File | Copy-Item -Destination "$destinationPath"
} else {
    write-host "$destinationPath not found."
}



