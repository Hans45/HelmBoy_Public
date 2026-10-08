#region variables
$baseFolder = '../Patches/Factory Presets/Templates - '

$oscillatorsWaveForms = @(
    @{Name = "sin" ; Value = 0},
    @{Name = "triangle" ; Value = 1},
    @{Name = "square" ; Value = 2},
    @{Name = "saw up" ; Value = 3},
    @{Name = "saw down" ; Value = 4},
    @{Name = "3 step" ; Value = 5},
    @{Name = "4 step" ; Value = 6},
    @{Name = "8 step" ; Value = 7},
    @{Name = "3 pyramid" ; Value = 8},
    @{Name = "5 pyramid" ; Value = 9},
    @{Name = "9 pyramid" ; Value = 10}
)

$filterTypes = @(
    @{Name = "12 db LP" ; Style = '0' ; Blend = '0'; Shelf = '0' ; On = '1.0'},
    @{Name = "12 db BP" ; Style = '0' ; Blend = '1'; Shelf = '0' ; On = '1.0'},
    @{Name = "12 db HP" ; Style = '0' ; Blend = '2'; Shelf = '0' ; On = '1.0'},
    @{Name = "24 db LP" ; Style = '1' ; Blend = '0'; Shelf = '0' ; On = '1.0'},
    @{Name = "24 db BP" ; Style = '1' ; Blend = '1'; Shelf = '0' ; On = '1.0'},
    @{Name = "24 db HP" ; Style = '1' ; Blend = '2'; Shelf = '0' ; On = '1.0'},
    @{Name = "band shelf" ; Style = '2' ; Blend = '0'; Shelf = '1' ; On = '1.0'},
    @{Name = 'high shelf' ; Style = '2' ; Blend = '0'; Shelf = '2' ; On = '1.0' },
    @{Name = "low shelf" ; Style = '2' ; Blend = '0'; Shelf = '0' ; On = '1.0'},
    @{Name = "none" ; Style = '0' ; Blend = '0'; Shelf = '0' ; On = '0.0'}
)

$patchContent = @"
{
  "license": "Patch (c) by Marc Scheffer.  This patch is licensed under a Creative Commons Attribution 4.0 International License.  You should have received a copy of the license along with this work.  If not, see http://creativecommons.org/licenses/by/4.0/.",
  "synth_version": "0.9.1.0",
  "patch_name": "PATCHNAME",
  "folder_name": "FOLDERNAME",
  "author": "Marc Scheffer",
  "settings": {
    "amp_attack": 0.17,
    "amp_decay": 1.5,
    "amp_release": 1.95,
    "amp_sustain": 1,
    "arp_frequency": 2.0,
    "arp_gate": 0.5,
    "arp_octaves": 1.0,
    "arp_on": 0.0,
    "arp_pattern": 0.0,
    "arp_sync": 1.0,
    "arp_tempo": 9.0,
    "beats_per_minute": 2.0,
    "cross_modulation": 0.0,
    "cutoff": 80.0,
    "delay_dry_wet": 0.5,
    "delay_feedback": 0.4,
    "delay_frequency": 2.0,
    "delay_on": 0.0,
    "delay_sync": 1.0,
    "delay_tempo": 8.0,
    "distortion_drive": 0.0,
    "distortion_mix": 1.0,
    "distortion_on": 0.0,
    "distortion_type": 0.0,
    "fil_attack": 0.17,
    "fil_decay": 1.5,
    "fil_env_depth": 0.0,
    "fil_release": 1.95,
    "fil_sustain": 1,
    "filter_blend": BLENDVALUE,
    "filter_drive": 0.0,
    "filter_on": FILTERON,
    "filter_shelf": SHELFVALUE,
    "filter_style": FILTERTYPE,
    "formant_on": 0.0,
    "formant_x": 0.5,
    "formant_y": 0.5,
    "keytrack": 0.0,
    "legato": 0.0,
    "mod_attack": 0.17,
    "mod_decay": 1.5,
    "mod_release": 1.95,
    "mod_sustain": 1,
    "mono_lfo_1_amplitude": 1.0,
    "mono_lfo_1_frequency": 1.0,
    "mono_lfo_1_retrigger": 2.0,
    "mono_lfo_1_sync": 1.0,
    "mono_lfo_1_tempo": 6.0,
    "mono_lfo_1_waveform": 0.0,
    "mono_lfo_2_amplitude": 1.0,
    "mono_lfo_2_frequency": 1.0,
    "mono_lfo_2_retrigger": 2.0,
    "mono_lfo_2_sync": 1.0,
    "mono_lfo_2_tempo": 7.0,
    "mono_lfo_2_waveform": 0.0,
    "mono_lfo_3_amplitude": 1.0,
    "mono_lfo_3_frequency": 1.0,
    "mono_lfo_3_retrigger": 2.0,
    "mono_lfo_3_sync": 1.0,
    "mono_lfo_3_tempo": 7.0,
    "mono_lfo_3_waveform": 0.0,
    "mono_lfo_4_amplitude": 1.0,
    "mono_lfo_4_frequency": 1.0,
    "mono_lfo_4_retrigger": 2.0,
    "mono_lfo_4_sync": 1.0,
    "mono_lfo_4_tempo": 7.0,
    "mono_lfo_4_waveform": 0.0,
    "noise_volume": 0.0,
    "num_steps": 32.0,
    "osc_1_transpose": 0.0,
    "osc_1_tune": 0.016,
    "osc_1_unison_detune": 10.0,
    "osc_1_unison_voices": 1.0,
    "osc_1_volume": OSC1VOLUME,
    "osc_1_waveform": OSC1WAVEFORM,
    "osc_2_transpose": 0.0,
    "osc_2_tune": 0.008,
    "osc_2_unison_detune": 10.0,
    "osc_2_unison_voices": 1.0,
    "osc_2_volume": OSC2VOLUME,
    "osc_2_waveform": OSC2WAVEFORM,
    "osc_feedback_amount": 0.0,
    "osc_feedback_transpose": 0.0,
    "osc_feedback_tune": 0.0,
    "pitch_bend_range": 2.0,
    "poly_lfo_1_amplitude": 1.0,
    "poly_lfo_1_frequency": 1.0,
    "poly_lfo_1_sync": 1.0,
    "poly_lfo_1_tempo": 7.0,
    "poly_lfo_1_waveform": 0.0,
    "poly_lfo_2_amplitude": 1.0,
    "poly_lfo_2_frequency": 1.0,
    "poly_lfo_2_sync": 1.0,
    "poly_lfo_2_tempo": 7.0,
    "poly_lfo_2_waveform": 0.0,
    "polyphony": 8.0,
    "portamento": -7.0,
    "portamento_type": 0.0,
    "resonance": 0.5,
    "reverb_damping": 0.5,
    "reverb_dry_wet": 0.5,
    "reverb_feedback": 0.9,
    "reverb_on": 0.0,
    "step_frequency": 2.0,
    "step_seq_00": 0.0,
    "step_seq_01": 0.0,
    "step_seq_02": 0.0,
    "step_seq_03": 0.0,
    "step_seq_04": 0.0,
    "step_seq_05": 0.0,
    "step_seq_06": 0.0,
    "step_seq_07": 0.0,
    "step_seq_08": 0.0,
    "step_seq_09": 0.0,
    "step_seq_10": 0.0,
    "step_seq_11": 0.0,
    "step_seq_12": 0.0,
    "step_seq_13": 0.0,
    "step_seq_14": 0.0,
    "step_seq_15": 0.0,
    "step_seq_16": 0.0,
    "step_seq_17": 0.0,
    "step_seq_18": 0.0,
    "step_seq_19": 0.0,
    "step_seq_20": 0.0,
    "step_seq_21": 0.0,
    "step_seq_22": 0.0,
    "step_seq_23": 0.0,
    "step_seq_24": 0.0,
    "step_seq_25": 0.0,
    "step_seq_26": 0.0,
    "step_seq_27": 0.0,
    "step_seq_28": 0.0,
    "step_seq_29": 0.0,
    "step_seq_30": 0.0,
    "step_seq_31": 0.0,
    "step_seq_32": 0.0,
    "step_seq_33": 0.0,
    "step_seq_34": 0.0,
    "step_seq_35": 0.0,
    "step_seq_36": 0.0,
    "step_seq_37": 0.0,
    "step_seq_38": 0.0,
    "step_seq_39": 0.0,
    "step_seq_40": 0.0,
    "step_seq_41": 0.0,
    "step_seq_42": 0.0,
    "step_seq_43": 0.0,
    "step_seq_44": 0.0,
    "step_seq_45": 0.0,
    "step_seq_46": 0.0,
    "step_seq_47": 0.0,
    "step_seq_48": 0.0,
    "step_seq_49": 0.0,
    "step_seq_50": 0.0,
    "step_seq_51": 0.0,
    "step_seq_52": 0.0,
    "step_seq_53": 0.0,
    "step_seq_54": 0.0,
    "step_seq_55": 0.0,
    "step_seq_56": 0.0,
    "step_seq_57": 0.0,
    "step_seq_58": 0.0,
    "step_seq_59": 0.0,
    "step_seq_60": 0.0,
    "step_seq_61": 0.0,
    "step_seq_62": 0.0,
    "step_seq_63": 0.0,
    "step_seq_64": 0.0,
    "step_seq_65": 0.0,
    "step_seq_66": 0.0,
    "step_seq_67": 0.0,
    "step_seq_68": 0.0,
    "step_seq_69": 0.0,
    "step_seq_70": 0.0,
    "step_seq_71": 0.0,
    "step_seq_72": 0.0,
    "step_seq_73": 0.0,
    "step_seq_74": 0.0,
    "step_seq_75": 0.0,
    "step_seq_76": 0.0,
    "step_seq_77": 0.0,
    "step_seq_78": 0.0,
    "step_seq_79": 0.0,
    "step_seq_80": 0.0,
    "step_seq_81": 0.0,
    "step_seq_82": 0.0,
    "step_seq_83": 0.0,
    "step_seq_84": 0.0,
    "step_seq_85": 0.0,
    "step_seq_86": 0.0,
    "step_seq_87": 0.0,
    "step_seq_88": 0.0,
    "step_seq_89": 0.0,
    "step_seq_90": 0.0,
    "step_seq_91": 0.0,
    "step_seq_92": 0.0,
    "step_seq_93": 0.0,
    "step_seq_94": 0.0,
    "step_seq_95": 0.0,
    "step_seq_96": 0.0,
    "step_seq_97": 0.0,
    "step_seq_98": 0.0,
    "step_seq_99": 0.0,
    "step_seq_100": 0.0,
    "step_seq_101": 0.0,
    "step_seq_102": 0.0,
    "step_seq_103": 0.0,
    "step_seq_104": 0.0,
    "step_seq_105": 0.0,
    "step_seq_106": 0.0,
    "step_seq_107": 0.0,
    "step_seq_108": 0.0,
    "step_seq_109": 0.0,
    "step_seq_110": 0.0,
    "step_seq_111": 0.0,
    "step_seq_112": 0.0,
    "step_seq_113": 0.0,
    "step_seq_114": 0.0,
    "step_seq_115": 0.0,
    "step_seq_116": 0.0,
    "step_seq_117": 0.0,
    "step_seq_118": 0.0,
    "step_seq_119": 0.0,
    "step_seq_120": 0.0,
    "step_seq_121": 0.0,
    "step_seq_122": 0.0,
    "step_seq_123": 0.0,
    "step_seq_124": 0.0,
    "step_seq_125": 0.0,
    "step_seq_126": 0.0,
    "step_seq_127": 0.0,
    "step_sequencer_retrigger": 2.0,
    "step_sequencer_sync": 1.0,
    "step_sequencer_tempo": 7.0,
    "step_smoothing": 0.0,
    "stutter_frequency": 3.0,
    "stutter_on": 0.0,
    "stutter_resample_frequency": 1.0,
    "stutter_resample_sync": 1.0,
    "stutter_resample_tempo": 6.0,
    "stutter_softness": 0.2,
    "stutter_sync": 1.0,
    "stutter_tempo": 8.0,
    "sub_octave": 0.0,
    "sub_shuffle": 0.0,
    "sub_volume": 0.0,
    "sub_waveform": 2.0,
    "unison_1_harmonize": 0.0,
    "unison_2_harmonize": 0.0,
    "velocity_track": 0.0,
    "volume": 0.7071068,
    "modulations": []
  }
}
"@
#endregion variables
#region functions
#endregion functions
#region main
#Generation des noms de patches
$filterTypes | ForEach-Object {
    $filterType = $_
    $currentPatchFolder = "$baseFolder$($filterType.Name)"
    #Cr��er le dossier s'il n'existe pas
    if (-not (Test-Path -Path $currentPatchFolder)) {
        New-Item -ItemType Directory -Path $currentPatchFolder | Out-Null
    }
    $oscillatorsWaveForms | ForEach-Object {
        $osc1Waveform = $_
        #On commence par créer une série d'entrées avec 1 seul oscillateur actif.
        $osc2Waveform = $oscillatorsWaveForms[0]
        $patchFileName = "$($osc1Waveform.Name) only"
        $patchName = "$($osc1Waveform.Name) only $($filterType.Name) Template"
        $currentPatchContent = $patchContent
        $currentPatchContent = $currentPatchContent.replace('PATCHNAME', $patchName)
        $currentPatchContent = $currentPatchContent.Replace('FOLDERNAME', "$currentPatchFolder")
        $currentPatchContent = $currentPatchContent.Replace('FILTERON', $($filterType.On))
        $currentPatchContent = $currentPatchContent.Replace('FILTERTYPE', $($filterType.Style))
        $currentPatchContent = $currentPatchContent.Replace('OSC1WAVEFORM', $($osc1Waveform.Value))
        $currentPatchContent = $currentPatchContent.Replace('OSC2WAVEFORM', $($osc2Waveform.Value))
        $currentPatchContent = $currentPatchContent.Replace('BLENDVALUE', $($filterType.Blend))
        $currentPatchContent = $currentPatchContent.Replace('SHELFVALUE', $($filterType.Shelf))
        $currentPatchContent = $currentPatchContent.Replace('OSC1VOLUME', '1.0');
        $currentPatchContent = $currentPatchContent.Replace('OSC2VOLUME', '0.0');
        Write-Host -ForegroundColor Green "Creating patch file: $currentPatchFolder/$patchFileName.helmBoy"
        $currentPatchContent | Out-File -FilePath "$currentPatchFolder/$patchFileName.helmBoy" -Encoding UTF8

        #On crée ensuite la série d'entrées à 2 oscillateurs en évitant les doublons
        $oscillatorsWaveForms | ForEach-Object{
            if ($_.Value -ge $osc1Waveform.Value) { # Eviter les doublons en ne prenant que les combinaisons o�� osc2 >= osc1
                $osc2Waveform = $_
                $patchFileName = "$($osc1Waveform.Name) - $($osc2Waveform.Name)"
                $patchName = "$($osc1Waveform.Name)/$($osc2Waveform.Name) $($filterType.Name) Template"
                #G�n�rer ici le contenu du patch
                $currentPatchContent = $patchContent
                $currentPatchContent = $currentPatchContent.Replace('BLENDVALUE', $($filterType.Blend))
                $currentPatchContent = $currentPatchContent.Replace("FILTERON", $($filterType.On))
                $currentPatchContent = $currentPatchContent.Replace("FILTERTYPE", $($filterType.Style))
                $currentPatchContent = $currentPatchContent.Replace('FOLDERNAME', "$currentPatchFolder")
                $currentPatchContent = $currentPatchContent.Replace('OSC1VOLUME', '0.5477225575');
                $currentPatchContent = $currentPatchContent.Replace('OSC2VOLUME', '0.5477225575');
                $currentPatchContent = $currentPatchContent.Replace('OSC1WAVEFORM', $($osc1Waveform.Value))
                $currentPatchContent = $currentPatchContent.Replace("OSC2WAVEFORM", $($osc2Waveform.Value))
                $currentPatchContent = $currentPatchContent.replace('PATCHNAME', $patchName)
                $currentPatchContent = $currentPatchContent.Replace("SHELFVALUE", $($filterType.Shelf))
                #Enregistrer le patch dans un fichier
                Write-Host -ForegroundColor Green "Creating patch file: $currentPatchFolder/$patchFileName.helmBoy"
                $currentPatchContent | Out-File -FilePath "$currentPatchFolder/$patchFileName.helmBoy" -Encoding UTF8
            }
        }
    }
}

#endregion main
