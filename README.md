락 취득
1. 주변 노드에게 특정 id의 데이터에 대한 lock을 request하고 state을 waitfor로 변경
2. 메시지를 받은 노드는 주변 노드에게 request를 전파
3. leaf node는 해당 메시지를 allow
4. 자신이 보낸 모든 request가 allow되면 자신에게 request를 보낸 노드에게 allow를 보냄
5. 최초로 req를 보낸 노드가 다른 모든 노드에게 allow를 받으면 state을 locked로 변경, lock취득

reqLock의 충돌: 이미 reqLock을 보내고 allow를 대기하는 상황에서 새로운 reqLock이 도착
a. 기존 req의 우선도가 높음, 새로운req는 대기 (문제X)
b. 새로운 req가 우선도가 높음 (중요)
    1. 기존 req를 보낸 fd에게 reqLock전송, 해당 노드에 대해서는 allow받지 못했으므로
    2. 기존req의 allow를 새로운 req가 대신 받음, 우선도가 낮은 기존req가 allow됐다면 우선도가 더 높은 새로운 req는 당연히 allow됐을 것
    3. (1), (2)에 대한 모든 allow를 받으면 새로운 req에 대한 allow전송
    4. 이후 기존 req의 차례가 왔을 때 reqLock을 다시보냄

allowCnt: 다른 노드에게 allowLock을 보내기 위해 필요한 allow의 갯수
1. 다른 노드로 reqLock을 보낼때 전송받는 노드의 갯수만큼 값이 설정됨
2. 다른 노드로 부터 allow를 받을때마다 하나씩 감소함
3. 0이 된다면 이전 노드에게 allowLock전송

기존 노드A가 lock을 hold한 데이터를 또다른 노드B가 reqLock 한다면?
1. 새로운 reqLock은 다른 노드들에게 전송됨
2. A가 아닌 노드들은 해당 reqLock을 다른 노드에게 전파할 것
3. 다른 노드가 allow하더라도 A는 allow하지 않음, req를 reqTable에 저장함
5. 향후 A가 unlock될때 reqTable에서 다음 req에 대한 작업을 처리함

unlock: lock을 취득한 노드가 데이터 unlock
=> 데이터의 unlock을 굳이 다른 노드에게 알릴 필요는 없음, reqLock을 allow하지 않는 것 만으로도 lock을 지니고 있다는 것을 알릴 수 있음
=> 따라서 notifyUnlock은 필요없을 예정