param([ValidateSet('GameStart','GameClosed','DeviceTest','GameClosing')][string]$Kind='GameStart')
# User request (2026-10-05): one audible system sound right before a diagnostic game launch,
# and a different one after the game has exited, so the user can pause their own work.
# Plays a stock Windows sound synchronously; never changes audio settings.
# DeviceTest (user 2026-10-18 11:20: "I work with this chat in the back; when a headset test is needed, play the
# notification sound twice in a row"): the notification sound two times.
# GameClosing (user 2026-10-21 17:20: a Mock close took their open CLIP STUDIO down with it - "play a sound 10 seconds before
# the close, I will save"): the alarm sound, played by request_game_close.ps1 ten seconds before it closes the game.
$ErrorActionPreference='Stop'
$file=if($Kind -eq 'GameStart'){'Windows Exclamation.wav'}elseif($Kind -eq 'GameClosing'){'Alarm02.wav'}else{'Windows Notify System Generic.wav'}
$path=Join-Path $env:WINDIR "Media\$file"
try {
    if($Kind -eq 'DeviceTest'){$notify=Join-Path $env:WINDIR 'Media\Windows Notify System Generic.wav'
        if(Test-Path -LiteralPath $notify){$player=New-Object System.Media.SoundPlayer $notify;$player.PlaySync();Start-Sleep -Milliseconds 250;$player.PlaySync()}
        else{[System.Media.SystemSounds]::Asterisk.Play();Start-Sleep -Milliseconds 900;[System.Media.SystemSounds]::Asterisk.Play();Start-Sleep -Milliseconds 800}}
    elseif(Test-Path -LiteralPath $path){(New-Object System.Media.SoundPlayer $path).PlaySync()}
    elseif($Kind -eq 'GameStart'){[System.Media.SystemSounds]::Exclamation.Play();Start-Sleep -Milliseconds 800}
    else{[System.Media.SystemSounds]::Asterisk.Play();Start-Sleep -Milliseconds 800}
    Write-Output "sound $Kind played"
} catch { Write-Output "sound $Kind failed: $($_.Exception.Message)" }
