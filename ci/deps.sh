#!/bin/bash -eux
if [[ "$BUILD_TYPE" == "Docs" ]] ; then
  sudo apt-get update -qq
  sudo apt-get install -qq doxygen doxygen-doc doxygen-gui graphviz
  exit 0
fi

case "$TRAVIS_OS_NAME" in
  linux)
    if [[ "$BUILD_TYPE" == "RpiDocker" ]]; then
      sudo apt-get install -qq binfmt-support qemu qemu-user-static
      set +e
      sudo update-binfmts --enable qemu-arm
      docker pull iscore/iscore-rpi-sdk:latest
      set -e
    else
      sudo wget -nv https://github.com/OSSIA/score-sdk/releases/download/sdk7/boost.tar.bz2 -O /opt/boost.tar.bz2 &

      wget -nv https://cmake.org/files/v3.9/cmake-3.9.0-Linux-x86_64.tar.gz -O cmake-linux.tgz &

      echo 'deb http://apt.llvm.org/trusty/ llvm-toolchain-trusty-4.0 main' | sudo tee /etc/apt/sources.list.d/llvm.list
      sudo apt-key adv --recv-keys --keyserver keyserver.ubuntu.com 1397BC53640DB551
      sudo add-apt-repository --yes ppa:ubuntu-toolchain-r/test
      sudo add-apt-repository --yes ppa:beineri/opt-qt591-trusty
      sudo apt-get update -qq
      sudo apt-get install -qq --yes --force-yes g++-7 binutils ninja-build gcovr lcov qt59-meta-minimal libasound2-dev clang-4.0 lld-4.0

      wait wget || true

      (cd /opt; sudo tar xaf boost.tar.bz2; sudo mv boost_* boost ; sudo chmod -R a+rwx boost)

      tar xaf cmake-linux.tgz
      mv cmake-*-x86_64 cmake
    fi

    shopt -s nocasematch # case insensitive comparison in Bash
    if [[ "$BUILD_TYPE" == Rpi* ]]; then
        # install arm-linux-gnueabihf-g++-6 from yaketty
        pushd /etc/apt
        sudo cp /etc/apt/sources.list /etc/apt/sources.list_bak
        sudo sed -i -- 's/trusty/yakkety/g' sources.list
        sudo apt-get update -qq
        sudo apt-get install -qq g++-6-arm-linux-gnueabihf
        sudo cp /etc/apt/sources.list_bak /etc/apt/sources.list
        popd

        # download, extract and mount raspberry pi image with gcc-6 installed
        wget -nv https://www.dropbox.com/s/o61vwblh6b5ixu7/raspbian-stretch-ossia-full.img.tar.gz
        tar -xf raspbian-stretch-ossia-full.img.tar.gz
        mkdir -p /tmp/rpi/root
        sudo mount -o loop,offset=48234496,rw,sync raspbian-stretch-ossia-full.img /tmp/rpi/root/

        # Copy boost to system path and image path
        sudo ln -s /opt/boost/boost /usr/include/boost
    elif [[ $BUILD_TYPE == *python* ]]; then
      if [[ "$BUILD_TYPE" == "python_manylinux" ]]; then
        if [[ "${PRE_CMD:-linux64}" == "linux32" ]]; then
          docker pull quay.io/pypa/manylinux1_i686
        else
          docker pull quay.io/pypa/manylinux1_x86_64
        fi
        wget https://bootstrap.pypa.io/get-pip.py
        # Install setuptools (need for build wheel)
        for PYBIN in /opt/python/*/bin/; do
            "${PYBIN}/python" get-pip.py
            "${PYBIN}/pip" install setuptools
        done
      else
        if [[ "$PYTHON_VERSION" == "2.7" ]]; then
          sudo apt-get update -qq
          sudo apt install -qq python python-dev python-pip
          sudo pip install twine
        else
          sudo add-apt-repository --yes ppa:jonathonf/python-3.6
          sudo apt-get update -qq
          sudo apt install -qq python3 python3-dev python3-pip
          sudo pip3 install twine
        fi
      fi
    fi
    if [[ "$BUILD_TYPE" == "RpiPythonRelease" ]]; then
      if [[ "$PYTHON_VERSION" == "2.7" ]]; then
        sudo pip install twine
      else
        sudo pip3 install twine
      fi
    fi
  ;;
  osx)
    # work around a homebrew bug
    set +e
    brew install gnu-tar xz
    ARCHIVE=homebrew-cache.tar.xz
    wget -nv "https://github.com/OSSIA/score-sdk/releases/download/sdk8/$ARCHIVE" -O "$ARCHIVE"
    gtar xhzf "$ARCHIVE" --directory /usr/local/Cellar
    brew link --force boost cmake ninja qt5

    if [[ "$BUILD_TYPE" == "MaxRelease" ]]; then
      mkdir -p "$HOME/Documents/Max 7/Packages"
      cd "$HOME/Documents/Max 7/Packages"
      MAXSDKARCHIVE=max-sdk-7.3.3.zip
      wget -nv "https://cycling74.s3.amazonaws.com/download/$MAXSDKARCHIVE"
      tar xf "$MAXSDKARCHIVE"
      ls
    elif [[ "x$BUILD_TYPE" == "xpython" || $BUILD_TYPE == *Pd* ]]; then
      if [[ "x$PYTHON_VERSION" == x2.* ]]; then
        brew install python2
        pip install wheel --user
        pip install twine --user
      else
        brew install python3
        pip3 install wheel --user
        pip3 install twine --user
      fi
    fi

    set -e
  ;;
esac
