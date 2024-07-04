# System_programming

User의 입장에서 어떻게 OS 커널과 상호작용하는지에 대해 직접 쉘 프로그램, 주식 서버, malloc 을 구현하였다.

*** 각각의 디렉토리 내부에 document 문서를 참고하면 프로젝트에 대해 자세한 설명을 확인할 수 있습니다.

## Project1 
### 미니 쉘 프로그램 구현
이 프로젝트는 cd, ls, mkdir, history 등의 명령어를 실행하는 기본적인 미니 쉘을 구현하는 것을 목표로 한다. fork와 execvp를 이용해 자식 프로세스를 생성하여 명령어를 실행하고, 파이프(|)와 백그라운드(&) 명령어도 처리하도록 구현하였다. history 명령어는 이전 명령어 기록을 파일에 저장하여 관리하며, 백그라운드 명령어는 쉘이 계속 작동할 수 있도록 처리하였다.

### Phase 1: cd, ls, mkdir, history 등의 명령어를 실행하는 기본적인 미니 쉘 구현
- ls, mkdir, rmdir, touch, cat, echo는 fork를 사용해 자식 프로세스를 생성하고 execvp로 실행
- cd, history, exit는 쉘 내부에서 구현된 built-in 명령어로 처리
- history 명령어는 외부 파일 .history에 명령어 기록을 저장하여 !!, !#와 같은 명령어 실행 가능

### Phase 2: 파이프를 포함한 명령어 실행 구현
- 명령어를 파이프로 구분하여 처리하고, 파이프의 입력과 출력을 적절히 연결
- 부모 프로세스에서 파이프 연결을 관리하고 자식 프로세스들 간의 입력과 출력을 파이프로 연결
  
### Phase 3: 백그라운드에서 명령어 실행 구현
- 백그라운드 명령어 실행 시 부모 프로세스가 자식 프로세스를 기다리지 않고 쉘이 계속 입력을 받을 수 있도록 구현
- 백그라운드 작업을 관리하기 위해 jobs, bg, fg, kill 명령어를 추가로 구현

---

## Project2
### 주식 서버 구현
이 프로젝트는 여러 클라이언트가 동시에 접속할 수 있는 Concurrent Stock Server를 구현하는 것이다. 이벤트 기반(Event-Driven) 방식과 스레드 기반(Thread-Driven) 방식으로 총 두 가지 접근 방식으로 설계되었다.

### 이벤트 기반(Event-Driven) 방식 
- Binary Tree를 사용해 주식 정보를 저장
- 클라이언트의 파일 디스크립터를 select 함수를 통해 모니터링하여 동시 접속 처리
- 주식 정보 접근 시 동기화 문제 해결을 위해 Semaphore 사용

### 스레드 기반(Thread-Driven) 방식
- 마찬가지로 Binary Tree를 사용해 주식 정보를 저장
- 미리 생성된 스레드 풀을 사용하여 클라이언트 요청을 처리
- 버퍼를 이용해 클라이언트의 요청을 각 스레드가 처리하며, sbuf 패키지를 이용해 동기화 문제 해결
  
### 성능 평가
- 클라이언트 수와 요청 수를 변화시키며 처리율과 실행 시간을 측정
- 이벤트 기반 방식은 클라이언트 수가 증가할수록 동시 처리율이 감소하는 경향이 있으나, 스레드 기반 방식은 증가하는 클라이언트 수에 따라 동시 처리율이 오히려 증가
- show 요청이 가장 시간이 많이 걸리고, buy/sell 요청이 가장 빠르게 처리됨

  

---

## Project3
### malloc 구현
이 프로젝트에서는 동적 할당 기능을 제공하는 malloc 패키지를 explicit list 방식으로 구현하였다. Fragmentation을 최소화하며 빠르고 효율적인 동적 할당을 제공하는 malloc을 만드는 것이 본 프로젝트의 목표이다.

### implicit list vs explicit list
- 각각은 동적 할당에 관하여 힙 영역에서 어떻게 Free block들을 관리하며 메모리를 할당 및 해제할 것인지에 대한 방법론이다.
- implicit list 방식은 순차적으로 모든 블록을 검사하는 선형적인 브루트 포스 방식으로 header, footer 와 같은 할당 정보와 할당 크기를 담은 추가 블록이 필요하다.
- explicit list 방식은 Free Block 들을 연결 리스트로 관리하는 방식으로 implicit list 방식에 비해 Troughput을 효과적으로 개선할 수 있다.

