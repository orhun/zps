![Logo](https://user-images.githubusercontent.com/24392180/66733887-b52b4780-ee69-11e9-86ee-ab04caf98287.png)

# zps [![Release](https://img.shields.io/github/release/orhun/zps.svg?color=590000&style=flat-square)](https://github.com/orhun/zps/releases)

### GNU/Linux에서 좀비 프로세스를 나열하고 끄는(reaping) 작은 유틸리티성 프로그램

![zps](https://user-images.githubusercontent.com/24392180/66898210-64e6ed80-f001-11e9-8e27-0ab3a7cabdca.gif)

[![Build](https://img.shields.io/github/workflow/status/orhun/zps/Continuous%20Integration?color=black&style=flat-square)](https://github.com/orhun/zps/actions?query=workflow%3A%22Continuous+Integration%22)
[![Docker Build](https://img.shields.io/github/workflow/status/orhun/zps/Docker%20Automated%20Builds?color=black&style=flat-square&label=docker)](https://github.com/orhun/zps/actions?query=workflow%3A%22Docker+Automated+Builds%22)
[![Codacy](https://img.shields.io/codacy/grade/3d40a551806b4c788befba6d2920675b.svg?color=black&style=flat-square)](https://www.codacy.com/manual/orhun/zps?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=orhun/zps&amp;utm_campaign=Badge_Grade)
[![Codecov](https://img.shields.io/codecov/c/github/orhun/zps?color=black&style=flat-square)](https://codecov.io/gh/orhun/zps)
[![Stars](https://img.shields.io/github/stars/orhun/zps.svg?color=590000&style=flat-square)](https://github.com/orhun/zps/stargazers) [![License](https://img.shields.io/github/license/orhun/zps.svg?color=590000&style=flat-square)](./LICENSE)

Unix와 컴퓨터 운영체제와 같은 Unix에서, [좀비 프로세스](https://en.wikipedia.org/wiki/Zombie_process) 또는 Defunct 프로세스는 (시스템 호출 [종료](https://en.wikipedia.org/wiki/Exit_(system_call))을 통해 이루어진) 종료가 되어도 프로세스 목록에 남아 있습니다. 이 현상은 부모 프로세스가 (시스템 호출 [중단](https://en.wikipedia.org/wiki/Wait(system_call))을 통해) 자식의 종료 상태를 알 필요가 있는 목록에서 자식 프로세스들에게 발생됩니다. 그렇게 좀비 프로세스를 목록에서 제거할 수 있었고, 이것을 "reaped"라고 합니다.

일반 프로세스들과 다르게, 좀비프로세스들은 이미 죽었기 때문에 [Kill](https://en.wikipedia.org/wiki/Kill_(command)) 명령어로 좀비프로세스들을 시스템에서 제거할 수 없습니다. (This is where the term's metaphor [zombie - an undead person] comes from.) 좀비 프로세스를 끄기 위해서, 'SIGCHLD' 신호를 부모 프로세스에게 [Kill](https://en.wikipedia.org/wiki/Kill_(command)) 명령어로 보내야 한다. 만약, 부모 프로세스가 좀비 프로세스를 끄는 것을 방해하는 경우에, 부모 프로세스는 대부분 'SIGTERM' 신호로 종료할 수 있는 방법도 있다. 자식 프로세스가 부모 프로세스를 잃었을 때, [init](https://en.wikipedia.org/wiki/Init) 프로세스(PID가 1번인 프로세스, 모든 프로세스의 조상 역할을 함.)가 자식 프로세스의 새로운 조상이 되고, 그것은 좀비 프로세스들을 시스템 호출 [중단](https://en.wikipedia.org/wiki/Wait_(system_call))을 해서 끌 수 있습니다.

좀비 프로세스들은 다른 프로세스들에 영향을 끼치거나 시스템 자원을 사용하지 않기 때문에 해롭지 않습니다. 하지만, 그들은 [프로세스 ID](https://en.wikipedia.org/wiki/Process_identifier)를 소유하고 있습니다. 이것은 모든 사용 가능한 PID를 좀비 프로세스가 차지하고 있다면 새로운 프로세스를 실행하는 것이 불가능 할 수 있습니다. Unix와 같은 시스템들에서는 프로세스 ID (`/proc/sys/kernel/pid_max`)를 한정된 양(제 우분투에서는 32768이 최댓값이네요.)만 갖고 있습니다. 이것만이 좀비 프로세스가 일으킬 수 있는 문제점입니다. 좀비 프로세스의 다른 위험성은 만약 좀비 프로세스가 프로세스 목록에 오랫동안 머무른다면, [메모리 누수](https://en.wikipedia.org/wiki/Resource_leak)를 일으킬 수 있다는 것입니다. 이러한 문제 외에도, 적은 양의 좀비 프로세스는 그들의 부모 프로세스에 버그를 생성할 지라도 시스템에 큰 영향을 주지는 않을 것입니다.

[zproc.c](https://github.com/orhun/zps/blob/master/example/zproc.c) 파일을 컴파일 할 수 있고, 어떻게 좀비 프로세스가 생성되는지 확인할 수 있습니다.
```
cd example/ && gcc -O3 -Wall zproc.c -o zproc && ./zproc
```

특정 시간에 실행하고 있는 프로세스들의 정보의 리스트와 좀비 프로세스들을 표시하기 위해 __zps__를 만들었습니다. 이 프로그램은 `--reap` 옵션을 사용했을 때, 자동으로 좀비 프로세스들을 끌 수 있습니다. 프로세스 리스트를 나열하기 전에 좀비 프로세스를 끄는 `--lreap` 옵션도 존재합니다. 좀 더 자세한 정보를 보려면 [사용법](https://github.com/orhun/zps#usage)을 보세요.
기술적으로, __zps__는 [/proc](https://www.tldp.org/LDP/Linux-Filesystem-Hierarchy/html/proc.html) 파일 시스템에서 프로세스 정보를 얻어오고, 프로세스를 출력하고 신호를 보내고 이외의 다른 동작들을 하기 위해 [C POSIX library](https://en.wikipedia.org/wiki/C_POSIX_library)를 이용합니다.

  - [설치 방법](#설치-방법)
    - [Arch Linux](#arch-linux)
    - [CMake](#cmake)
    - [Make](#make)
    - [GCC](#gcc)
    - [Docker](#docker)
      - [이미지 생성](#이미지-생성)
      - [컨테이너에서 이미지 실행](#컨테이너에서-이미지-실행)
  - [사용법](#사용법)
    - [zps -r](#zps--r)
    - [zps -x](#zps--x)
    - [zps -l](#zps--l)
    - [zps -p](#zps--p)
  - [TODO(s)](#todos)
  - [License](#license)
  - [Copyright](#copyright)

## 설치 방법

### Arch Linux

```
pacman -S zps
```

### CMake

```
mkdir -p build && cd build
cmake ../ -DCMAKE_INSTALL_PREFIX=/usr
make
sudo make install
sudo ldconfig
```

### Make

```
make
sudo make install
```

### GCC

```
cd src/ && gcc -s -O3 -Wall -Wextra -pedantic zps.c -o zps
```

### Docker

#### 이미지 생성

```
docker build -f docker/Dockerfile -t zps .
```

#### 컨테이너에서 이미지 실행

```
docker run zps
```

## 사용법

```
사용법:
  zps [options]

옵션:
  -r, --reap      좀비 프로세스 종료하기
  -x, --lreap     프로세스 리스트 나열하고, 좀비 프로세스 종료하기
  -l, --list      좀비 프로세스들만 목록에 보이게 하기
  -p, --prompt    선택된 프로세스들만 Prompt 된 것을 보기
  -f, --fd <num>  파일 디스크립터 최댓값 설정하기 (기본값: 15)
  -s, --silent    silent 모드로 실행하기
  -v, --version   버전 보기
  -h, --help      사용법 보기
```

### zps -r

![zps -r](https://user-images.githubusercontent.com/24392180/66898345-b68f7800-f001-11e9-86d7-694772a46ab7.gif)

### zps -x

![zps -x](https://user-images.githubusercontent.com/24392180/66898624-34ec1a00-f002-11e9-9d5a-dde84c925119.gif)

### zps -l

![zps -l](https://user-images.githubusercontent.com/24392180/67201180-5f791100-f40e-11e9-8ff6-fcbbca443e9a.gif)

### zps -p

![zps -p](https://user-images.githubusercontent.com/24392180/67624534-3c999300-f83a-11e9-95e4-46c3ce586197.gif)

## TODO(s)

* 긴 이름을 가진 프로세스를 출력하는 것을 보완하는 것.
* 종료하는 것 대신에 부모 프로세스에게 `SIGCHLD` 신호를 보내는 것.

## License

GNU General Public License v3.0 only ([GPL-3.0-only](https://www.gnu.org/licenses/gpl.txt))

## Copyright

Copyright © 2019-2023, [Orhun Parmaksız](mailto:orhunparmaksiz@gmail.com)

Translated to Korean by ahdelron.
