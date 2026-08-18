$ErrorActionPreference = 'Stop';

$packageName= 'approx'
$toolsDir   = "$(Split-Path -parent $MyInvocation.MyCommand.Definition)"
$url64      = 'https://github.com/riccivr/approx/releases/download/v1.0.0/approx-windows-amd64.zip'

$packageArgs = @{
  packageName   = $packageName
  unzipLocation = $toolsDir
  url64bit      = $url64
  softwareName  = 'approx*'
  checksum64    = 'SKIP'
  checksumType64= 'sha256'
}

Install-ChocolateyZipPackage @packageArgs
