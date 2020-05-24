/**********************************
 * FILE NAME: MP1Node.cpp
 *
 * DESCRIPTION: Membership protocol run by this Node.
 * 				Definition of MP1Node class functions.
 **********************************/

#include "MP1Node.h"

/*
 * Note: You can change/add any functions in MP1Node.{h,cpp}
 */

/**
 * Overloaded Constructor of the MP1Node class
 * You can add new members to the class if you think it
 * is necessary for your logic to work
 */
MP1Node::MP1Node(Member *member, Params *params, EmulNet *emul, Log *log, Address *address) {
	for( int i = 0; i < 6; i++ ) {
		NULLADDR[i] = 0;
	}
	this->memberNode = member;
	this->emulNet = emul;
	this->log = log;
	this->par = params;
	this->memberNode->addr = *address;
}

/**
 * Destructor of the MP1Node class
 */
MP1Node::~MP1Node() {}

/**
 * FUNCTION NAME: recvLoop
 *
 * DESCRIPTION: This function receives message from the network and pushes into the queue
 * 				This function is called by a node to receive messages currently waiting for it
 */
int MP1Node::recvLoop() {
    if ( memberNode->bFailed ) {
    	return false;
    }
    else {
    	return emulNet->ENrecv(&(memberNode->addr), enqueueWrapper, NULL, 1, &(memberNode->mp1q));
    }
}

/**
 * FUNCTION NAME: enqueueWrapper
 *
 * DESCRIPTION: Enqueue the message from Emulnet into the queue
 */
int MP1Node::enqueueWrapper(void *env, char *buff, int size) {
	Queue q;
	return q.enqueue((queue<q_elt> *)env, (void *)buff, size);
}

/**
 * FUNCTION NAME: nodeStart
 *
 * DESCRIPTION: This function bootstraps the node
 * 				All initializations routines for a member.
 * 				Called by the application layer.
 */
void MP1Node::nodeStart(char *servaddrstr, short servport) {
    Address joinaddr;
    joinaddr = getJoinAddress();

    // Self booting routines
    if( initThisNode(&joinaddr) == -1 ) {
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "init_thisnode failed. Exit.");
#endif
        exit(1);
    }

    if( !introduceSelfToGroup(&joinaddr) ) {
        finishUpThisNode();
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "Unable to join self to group. Exiting.");
#endif
        exit(1);
    }

    return;
}

/**
 * FUNCTION NAME: initThisNode
 *
 * DESCRIPTION: Find out who I am and start up
 */
int MP1Node::initThisNode(Address *joinaddr) {
	/*
	 * This function is partially implemented and may require changes
	 */
	int id = *(int*)(&memberNode->addr.addr);
	int port = *(short*)(&memberNode->addr.addr[4]);
    std::cout << "This node id is" << id << "port is " << port << std::endl;
	memberNode->bFailed = false;
	memberNode->inited = true;
	memberNode->inGroup = false;
    // node is up!
	memberNode->nnb = 0;
	memberNode->heartbeat = 0;
	memberNode->pingCounter = TFAIL;
	memberNode->timeOutCounter = -1;
    initMemberListTable(memberNode);

    return 0;
}

/**
 * FUNCTION NAME: introduceSelfToGroup
 *
 * DESCRIPTION: Join the distributed system
 */
int MP1Node::introduceSelfToGroup(Address *joinaddr) {
	MessageHdr *msg;
#ifdef DEBUGLOG
    static char s[1024];
#endif

    if ( 0 == memcmp((char *)&(memberNode->addr.addr), (char *)&(joinaddr->addr), sizeof(memberNode->addr.addr))) {
        // I am the group booter (first process to join the group). Boot up the group
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "Starting up group...");
#endif
        memberNode->inGroup = true;
    }
    else {
        /*size_t msgsize = sizeof(MessageHdr) + sizeof(joinaddr->addr) + sizeof(long) + 1;
        msg = (MessageHdr *) malloc(msgsize * sizeof(char));

        // create JOINREQ message: format of data is {struct Address myaddr}
        msg->msgType = JOINREQ;
        memcpy((char *)(msg+1), &memberNode->addr.addr, sizeof(memberNode->addr.addr));
        memcpy((char *)(msg+1) + 1 + sizeof(memberNode->addr.addr), &memberNode->heartbeat, sizeof(long));*/
        msg = new MessageHdr();
        msg->msgType = JOINREQ;
        msg->members = memberNode->memberList;
        msg->addr = &memberNode->addr;


#ifdef DEBUGLOG
        sprintf(s, "Trying to join...");
        log->LOG(&memberNode->addr, s);
#endif

        // send JOINREQ message to introducer member
        emulNet->ENsend(&memberNode->addr, joinaddr, (char *)msg, sizeof(MessageHdr));

        free(msg);
    }

    return 1;

}

/**
 * FUNCTION NAME: finishUpThisNode
 *
 * DESCRIPTION: Wind up this node and clean up state
 */
int MP1Node::finishUpThisNode(){
   /*
    * Your code goes here
    */
   return 0;
}

/**
 * FUNCTION NAME: nodeLoop
 *
 * DESCRIPTION: Executed periodically at each member
 * 				Check your messages in queue and perform membership protocol duties
 */
void MP1Node::nodeLoop() {
    if (memberNode->bFailed) {
    	return;
    }

    // Check my messages
    checkMessages();

    // Wait until you're in the group...
    if( !memberNode->inGroup ) {
    	return;
    }

    // ...then jump in and share your responsibilites!
    nodeLoopOps();

    return;
}

/**
 * FUNCTION NAME: checkMessages
 *
 * DESCRIPTION: Check messages in the queue and call the respective message handler
 */
void MP1Node::checkMessages() {
    void *ptr;
    int size;

    // Pop waiting messages from memberNode's mp1q
    while ( !memberNode->mp1q.empty() ) {
    	ptr = memberNode->mp1q.front().elt;
    	size = memberNode->mp1q.front().size;
    	memberNode->mp1q.pop();
    	recvCallBack((void *)memberNode, (char *)ptr, size);
    }
    return;
}

/**
 * FUNCTION NAME: recvCallBack
 *
 * DESCRIPTION: Message handler for different message types
 */
bool MP1Node::recvCallBack(void *env, char *data, int size ) {
	/*
	 * Your code goes here
	 */
	MessageHdr* msg = (MessageHdr*) data; //todo verify
	if(msg->msgType == JOINREQ) {
	    std::cout << "recevied JOINREQ MSG" <<  size << std::endl;
        printf("before print addr");
	  //  printAddress(msg->addr);
	    pushMember(msg);
	    sendMessage(msg->addr, JOINREP);
	}else if(msg->msgType == JOINREP) {
        std::cout << "received JOINREP MSG" << std::endl;
	    memberNode->inGroup = true;
	}
	else if(msg->msgType == PING) {
        std::cout << "received PING MSG" << std::endl;
	    handlePing(msg);
	}
	//delete msg;  //verify
	return true;
}

void MP1Node::update_src_member(MessageHdr* msg){
    MemberListEntry* src_member = check_member_list(msg->addr);
    if(src_member != nullptr){
        src_member->heartbeat ++;
        src_member->timestamp = par->getcurrtime();
    }else{
        pushMember(msg);
    }
}

void MP1Node::handlePing(MessageHdr* msg) {
    //update header
    update_src_member(msg);
    std::cout << "after update header" << std::endl;
    for(size_t i=0; i < msg->members.size(); i++){
        if( msg->members[i].id > 10 || msg->members[i].id < 0) {
            continue;
        }
        MemberListEntry* node = check_member_list(msg->members[i].id, msg->members[i].port);
        if(node != nullptr){
            if(msg->members[i].heartbeat > node->heartbeat){
                node->heartbeat = msg->members[i].heartbeat;
                node->timestamp = par->getcurrtime();
            }
        }else{
            pushMember(&msg->members[i]);
            std::cout << "finish push for index" << i << std::endl;
        }
    }
    std::cout << "finish handling ping" << std::endl;
}

void MP1Node::sendMessage(Address* toAddr, MsgTypes type) {
    if(isNullAddress(toAddr)){
        return;
    }
    std::cout << "in sendMessage" << std::endl;
    MessageHdr* msg = new MessageHdr();
    msg->msgType = type;
    msg->members = memberNode->memberList;
    std::cout << "msg addr " << memberNode->addr.getAddress() << std::endl;
    msg->addr = &memberNode->addr;
    emulNet->ENsend(&memberNode->addr, toAddr, (char *)msg, sizeof(MessageHdr));
    delete msg;
}

void MP1Node::pushMember(MessageHdr* msg) {
    int id=0;
    short port;
    std::cout << "in push member" << std::endl;
    printAddress(msg->addr);
    memcpy(&id, &(msg->addr->addr[0]), sizeof(int));
    std::cout << "id" << id << std::endl;
    memcpy(&port, &msg->addr->addr[4], sizeof(short));
    std::cout << port << std::endl;
    std::cout << "in push member after copy address" << std::endl;
    if(check_member_list(id, port)) {
        return;
    }

    MemberListEntry mem (id, port, 1, par->getcurrtime());
    memberNode->memberList.push_back(mem);
    log -> logNodeAdd(&memberNode->addr, msg->addr);

    std::cout << "finished push member" << std::endl;
}

Address* MP1Node::getAddress(int id, short port) {
    Address* address = new Address();
    memcpy(&address[0], &id, sizeof(int));
    memcpy(&address[4], &port, sizeof(short));
    return address;
}

void MP1Node::pushMember(MemberListEntry* e) {
    std::cout << "in push member for memberlist" << std::endl;
    Address* addr = getAddress(e->id, e->port);

    if (*addr == memberNode->addr) {
        delete addr;
        std::cout << "end pushmember" << std::endl;
        return;
    }

    if (par->getcurrtime() - e->timestamp < TREMOVE) {
        std::cout << "add node  " << std::endl;
        log->logNodeAdd(&memberNode->addr, addr);
        memberNode->memberList.push_back(*e);
    }
    delete addr;
    std::cout << "end pushmember" << std::endl;
}


MemberListEntry* MP1Node::check_member_list(int id, short port) {
    for (size_t i = 0; i < memberNode->memberList.size(); i++){
        if(memberNode->memberList[i].id == id && memberNode->memberList[i].port == port)
            return &memberNode->memberList[i];
    }
    return nullptr;
}

MemberListEntry* MP1Node::check_member_list(Address* node_addr) {
    for (int i = 0; i < memberNode->memberList.size(); i++) {
        int id = 0;
        short port = 0;
        memcpy(&id, &node_addr->addr[0], sizeof(int));
        memcpy(&port, &node_addr->addr[4], sizeof(short));
        if (memberNode->memberList[i].id == id && memberNode->memberList[i].port == port)
            return &memberNode->memberList[i];
    }
    return nullptr;
}

/**
 * FUNCTION NAME: nodeLoopOps
 *
 * DESCRIPTION: Check if any node hasn't responded within a timeout period and then delete
 * 				the nodes
 * 				Propagate your membership list
 */
void MP1Node::nodeLoopOps() {

	/*
	 * Your code goes here
	 */
	memberNode->heartbeat ++;
    for (int i = memberNode->memberList.size()-1 ; i >= 0; i--) {
        if(par->getcurrtime() - memberNode->memberList[i].timestamp >= TREMOVE) {
            Address* removed_addr = getAddress(memberNode->memberList[i].id, memberNode->memberList[i].port);
            log->logNodeRemove(&memberNode->addr, removed_addr);
            memberNode->memberList.erase(memberNode->memberList.begin()+i);
            delete removed_addr;
        }
    }

    // Send PING to the members of memberList
    for (size_t i = 0; i < memberNode->memberList.size(); i++) {
        Address* address = getAddress(memberNode->memberList[i].id, memberNode->memberList[i].port);
        sendMessage(address, PING);
        delete address;
    }
    return;
}

/**
 * FUNCTION NAME: isNullAddress
 *
 * DESCRIPTION: Function checks if the address is NULL
 */
int MP1Node::isNullAddress(Address *addr) {
	return (memcmp(addr->addr, NULLADDR, 6) == 0 ? 1 : 0);
}

/**
 * FUNCTION NAME: getJoinAddress
 *
 * DESCRIPTION: Returns the Address of the coordinator
 */
Address MP1Node::getJoinAddress() {
    Address joinaddr;

    memset(&joinaddr, 0, sizeof(Address));
    *(int *)(&joinaddr.addr) = 1;
    *(short *)(&joinaddr.addr[4]) = 0;

    return joinaddr;
}

/**
 * FUNCTION NAME: initMemberListTable
 *
 * DESCRIPTION: Initialize the membership list
 */
void MP1Node::initMemberListTable(Member *memberNode) {
	memberNode->memberList.clear();
}

/**
 * FUNCTION NAME: printAddress
 *
 * DESCRIPTION: Print the Address
 */
void MP1Node::printAddress(Address *addr)
{
    std::cout << ("inside print addr") <<std::endl;
    printf("%d.%d.%d.%d:%d \n",  addr->addr[0],addr->addr[1],addr->addr[2],
                                                       addr->addr[3], *(short*)&addr->addr[4]) ;    
}
