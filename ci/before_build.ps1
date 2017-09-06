Set-PSDebug -Trace 1

cd  C:\projects\libossia
git submodule update --init --recursive

if ( $env:APPVEYOR_BUILD_TYPE -eq "max" ){
  curl --retry 10 -sO https://cycling74.s3.amazonaws.com/download/max-sdk-7.3.3.zip
  7z x max-sdk-7.3.3.zip -y > nul
}

if ( $env:APPVEYOR_BUILD_TYPE -eq "testing" ){
  if ( Test-Path ${env:QTDIR}\bin\ ) {
    set $env:PATH=${env:QTDIR}\bin;${env:PATH};
  }
}

mkdir build
cd build

# ok: cmake -G "Visual Studio 15 2017 Win64" -DCMAKE_BUILD_TYPE=Release c:\projects\libossia
cmake -G "${env:CMAKE_GENERATOR_NAME}" -DCMAKE_BUILD_TYPE=${env:configuration} c:\projects\libossia
# Write-Host cmake -G "${env:CMAKE_GENERATOR_NAME}" ${env:CMAKE_OPTION}  .. ">" C:\projects\libossia\config-${env:APPVEYOR_BUILD_TYPE}.log
# cmake -G "${env:CMAKE_GENERATOR_NAME}" -DCMAKE_BUILD_TYPE=${env:configuration} ${env:CMAKE_OPTION}  C:\projects\libossia  > C:\projects\libossia\config-${env:APPVEYOR_BUILD_TYPE}.log
exit
if ("$env:APPVEYOR_BUILD_TYPE" -eq "max"){
  cd ..
  mkdir build-32bit
  cd build-32bit
  mkdir xml
  Write-Host cmake -G ${env:CMAKE_GENERATOR_NAME_32BIT} ${env:CMAKE_OPTION}  .. > C:\projects\libossia\config-${env:APPVEYOR_BUILD_TYPE}-32bit.log
  cmake -G ${env:CMAKE_GENERATOR_NAME} -DCMAKE_BUILD_TYPE=${env:CONFIGURATION} ${env:CMAKE_OPTION}  C:\projects\libossia > C:\projects\libossia\config-${env:APPVEYOR_BUILD_TYPE}-32bit.log
  cd ../build
}