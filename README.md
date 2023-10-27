# 다음에 할 것

1. class node에 너무 많은것이 모여있음, lockTable관련 소스를 새로운 클래스로 구분하기 (lock.cpp, lock.h)

    => 좀 나중으로 미루기

2. 다른 프로세스에서 이 기능을 활용하게 할 방법 고민하기

    => malloc을 가상페이지 할당으로 바꾸고 공유메모리를 통해 관리? 데이터 접근은API?

# 락 취득

1. 주변 노드에게 특정 id의 데이터에 대한 lock을 reqLock하고 state을 waitfor로 변경

2. 메시지를 받은 노드는 주변 노드에게 reqLock를 전파

3. leaf node는 해당 메시지를 allow

4. 자신이 보낸 모든 reqLock가 allow되면 자신에게 reqLock를 보낸 노드에게 allow를 보냄

5. 최초로 req를 보낸 노드가 다른 모든 노드에게 allow를 받으면 state을 locked로 변경, lock취득

***lock의 취득은 노드들 사이에서의 권한만을 관여함, 노드 내부에서 다른 프로세스들 사이의 lock은 제어되지 않음

# 락 대기

=> acquireLock으로 lock을 요청하고, 대기하는 동안 어떤식으로 대기할 것인가?

a. whiel무한루프: 불필요한 cpu낭비?

b. mutex사용

    1. 사용자가 (특정 id 데이터에 대한)lock을 요구한다면 대상 id의 lock테이블이 대상에 대해 mtx_lock(앞서 서술된 lock과 다름)을 지님

    2. lock을 요구할 때 이미 mtx_lock이 있다면 아무것도 안함

    3. lock을 요구한 스레드는 mtx_lock을 요구한다.

    4. lock이 취득되면 lock테이블에서는 mtx_unlock

    => 자연스럽게 lock이 취득될 때 요청한 스레드에게 제어가 돌아옴

    => 데이터의 lock과 lockTable에서의 mtx_lock은 서로 반대

# reqLock의 충돌: 이미 reqLock을 보내고 allow를 대기하는 상황에서 새로운 reqLock이 도착

a. 기존 req의 우선도가 높음, 새로운req는 대기 (문제X)

b. 새로운 req가 우선도가 높음 (중요)
    
    1. 기존 reqA를 보낸 fd에게 reqLock전송, 해당 노드에 대해서는 allow받지 못했으므로
    
    2. A의 allow를 새로운 reqB가 대신 받음, 우선도가 낮은 A가 allow됐다면 우선도가 더 높은 B는 당연히 allow됐을 것
    
    3. (1), (2)에 대한 모든 allow를 받으면 B에 대한 allow전송
    
    4. 이후 A의 차례가 왔을 때 reqLock을 다시보냄

    => but 만약 또다른 reqC의 우선도가 A < C < B 라면?
    
    한쪽 노드(ㄱ)에선 B의 allow를 대기하고 다른 노드(ㄴ)에서는 C의 allow를 대기할 것임
    (ㄱ)은 B를 최우선으로 생각하지만 (ㄴ)은 reqB가 아닌 reqA를 받았으므로 C가 최우선이라 생각함
    
    => deadlock발생

# deadlock 해결방안?

a. 우선도가 A < C < B 인 reqLock C가 오면 allowCnt를 감소시킨다. (A보다 먼저, B보다 늦게 발생)

=> 다른 노드가 allow를 보냈다는 것은 해당 노드의 영역에서는 C가 없다는 것이므로 해당 reqLock C는 반드시 allow를 전송하지 않은 노드에서만 온다.

=> C와 B 이외에 A의 timeStamp를 알아야한다.

    a-1. reqTableNode에서 steal == 1 이라면 A의 timeStamp를 같이 저장한다. (stolenTimeStamp)

# allowCnt: 다른 노드에게 allowLock을 보내기 위해 필요한 allow의 갯수

1. 다른 노드로 reqLock을 보낼때 전송받는 노드의 갯수만큼 값이 설정됨

2. 다른 노드로 부터 allow를 받을때마다 하나씩 감소함

3. 0이 된다면 이전 노드에게 allowLock전송

# 기존 노드A가 lock을 hold한 데이터를 또다른 노드B가 reqLock 한다면?

1. 새로운 reqLock은 다른 노드들에게 전송됨

2. A가 아닌 노드들은 해당 reqLock을 다른 노드에게 전파할 것

3. 다른 노드가 allow하더라도 A는 allow하지 않음, req를 reqTable에 저장함

4. 향후 A가 unlock될때 reqTable에서 다음 req에 대한 작업을 처리함

# unlock: lock을 취득한 노드가 데이터 unlock

=> 데이터의 unlock을 굳이 다른 노드에게 알릴 필요는 없음, reqLock을 allow하지 않는 것 만으로도 lock을 지니고 있다는 것을 알릴 수 있음

=> 따라서 notifyUnlock은 필요없을 예정

# 데이터의 추가/삭제/수정

# 추가

=> 새로운 값을 추가할 때 사용하는 id는 모든 노드가 동일해야함, shareData::maxId에 대해 따로 lock?

# 삭제

=> 기존 data를 삭제한다고 해서 id값이 다시 작아지지는 않음(like pid), 그렇다면 각 id에 대한 데이터를 어떻게 저장할 것인가?

a. deque.at(id)

=> 데이터의 삭제가 이루어지면 빈 공간이 발생, 메모리누수

    => 향후 새로운 데이터가 추가될 때 maxId가 아닌 삭제된 이전 id 재활용?

b. hashTable

=> 데이터로의 상수 시간복잡도 접근 불가능

c. linkedList

=> 시간복잡도 O(n), hashTable 하위호환?

# 수정

=> lock을 취득한 노드가 특정 id 데이터에 대해 값을 수정 후 이를 전파, 상대적으로 간단