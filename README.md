# IOCP 기반 분산 서버 아키텍처 포트폴리오

## 프로젝트 개요
Windows IOCP(Input/Output Completion Port)를 활용한 고성능 분산 서버 시스템으로, MMORPG 서버 구조의 핵심 요소들을 구현했습니다. 세션 관리, 공간 분할 브로드캐스팅, 비동기 네트워크 처리 등 실제 게임 서버 개발에 필요한 주요 기술을 적용했습니다.

## 시스템 아키텍처

### 서버 구성

- **로그인 서버**: 인증 및 계정 관리
  - 세션키 발급 및 관리

- **채팅 서버**: 실시간 메시지 처리
  - 단일 스레드 모델 (ChatServerSingleThread)

- **게임 서버**: 게임 로직 처리
  - AuthGroup & GameGroup 기반 그룹 관리
  - 캐릭터 생성/이동 동기화

- **모니터링 서버**: 성형(Star) 토폴로지 기반 통합 모니터링
  - LAN 서버(정보 수집)와 NET 서버(모니터링) 분리 구현
  - 실시간 서버 상태 감시

### 클라이언트
- **GameChatDummy**: 서버 테스트용 더미 클라이언트
  - 로그인/채팅/게임 각 서버와의 통신 구현
  - TCP 통신 결과 검증

## 핵심 라이브러리 (Lib)

### 컨테이너
- **LockFreeQueue/Stack**: 락프리 큐/스택
- **ObjectPool**: 고성능 락프리 ObjectPool
- **RingBuffer**: 환형 버퍼 (소비자 : 생성자 = 1 : 1 / 락 safe 보장)
- **Job**: 잡큐의 Job

### 네트워크
- **NetServer/NetClient**: 기본 네트워크 서버/클라이언트 구현
- **LanServer/LanClient**: 내부 통신용 최적화 서버/클라이언트
- **MMOServer**: MMORPG 특화 서버 구현
- **Session**: 클라이언트 연결 관리
- **Group**: 세션 그룹 관리(MMOServer 리소스 분리 단위)
- **CPacket**: 패킷 처리(직렬화 버퍼)

### 데이터베이스
- **DBConnection**: 데이터베이스 연결 관리

### 도구
- **PacketGenerator**: IDL 기반 패킷 코드 자동 생성
  - Python 스크립트로 구현된 IDL 파서
  - 서버/클라이언트 양쪽 패킷 핸들러 자동 생성

## 기술적 특징

### 고성능 네트워크 처리
- Windows IOCP 모델 기반 비동기 I/O
- WSASend/WSARecv 비동기 처리 최적화
- Overlapped I/O 구현

### 안정성 및 확장성
- 로깅 시스템을 통한 상세한 서버 모니터링
- 서버 간 통신을 위한 표준화된 프로토콜 (CommonProtocol.h)
- 설정 파일 (.cnf) 기반 서버 구성 관리

### 모듈화 및 재사용성
- 공통 라이브러리를 통한 코드 중복 최소화
- 템플릿 기반 설계로 유연한 확장성 제공
- IDL 기반 패킷 자동 생성으로 개발 효율성 향상

---
