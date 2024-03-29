/**********************************
 * FILE NAME: MP1Node.cpp
 *
 * DESCRIPTION: Membership protocol run by this Node.
 * 				Definition of MP1Node class functions.
 **********************************/

#include "MP1Node.h"
#include "Log.h"

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
    
    memberNode->addr = Address(to_string(id) + ":" + to_string(port));
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
        //size_t msgsize = sizeof(MessageHdr) + sizeof(joinaddr->addr) + sizeof(long) + 1;
        //msg = (MessageHdr *) malloc(msgsize * sizeof(char));
        msg = new MessageHdr();
        // create JOINREQ message: format of data is {struct Address myaddr}
        msg->msgType = JOINREQ;
        msg->addr = &memberNode->addr;
        //memcpy((char *)(msg+1), &memberNode->addr.addr, sizeof(memberNode->addr.addr));
        //memcpy((char *)(msg+1) + 1 + sizeof(memberNode->addr.addr), &memberNode->heartbeat, sizeof(long));

#ifdef DEBUGLOG
        sprintf(s, "Trying to join...");
        log->LOG(&memberNode->addr, s);
#endif

        // send JOINREQ message to introducer member
        emulNet->ENsend(&memberNode->addr, joinaddr, (char *)msg, sizeof(MessageHdr));

        // put the node itself at the beginning of the membershipt list
        pushMember(msg);

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
    if(memberNode->bFailed) {
        return false;
    }
	MessageHdr* msg = (MessageHdr *) data;
    // Consume msg and construct response

    if (msg->msgType == JOINREQ){
        pushMember(msg);
        
        MessageHdr* replyMsg = new MessageHdr();
        replyMsg->msgType = JOINREP;
        replyMsg->members = memberNode->memberList;
        replyMsg->addr = &memberNode->addr;

        // reply with JOINREP message 
        emulNet->ENsend(&memberNode->addr, msg->addr, (char *)replyMsg, sizeof(MessageHdr));
    }
    else if(msg->msgType == JOINREP) {
        pushMember(msg);
        memberNode->inGroup = true;
    }
    else if(msg->msgType == PING) {
        handlePing(msg);
    }
    return true;
}

void MP1Node::handlePing(MessageHdr* msg) {
    for(const auto& updateMember: msg->members) {
        MemberListEntry* updateEntry = check_member_list(updateMember.id, updateMember.port); 
        if(updateMember.timestamp + TREMOVE < par->getcurrtime()) {
            continue;
        }
        if(!updateEntry) {
            pushMember(updateMember.id, updateMember.port);
        }
        else {
            if(updateEntry->heartbeat < updateMember.heartbeat) {
                        updateEntry->heartbeat = updateMember.heartbeat;
                        updateEntry->timestamp = par->getcurrtime();
            }
        }
    }
}

void MP1Node::pushMember(int id, short port) {
    memberNode->memberList.push_back({id, port, 0, par->getcurrtime()});  // heartbeat should be 0 as it's just joined
    auto addedAddress = Address(to_string(id) + ":" + to_string(port));
    log->logNodeAdd(&memberNode->addr, &addedAddress);
    return;
}

void MP1Node::pushMember(MessageHdr* msg) {
    int id = 0;
	short port;
	memcpy(&id, &msg->addr->addr[0], sizeof(int));
	memcpy(&port, &msg->addr->addr[4], sizeof(short));
    MemberListEntry* updateEntry = check_member_list(id, port);
    if(updateEntry) {
        return;
    }
    else {
       pushMember(id, port);
    }
}

MemberListEntry* MP1Node::check_member_list(int id, short port) {
    for(auto& member: memberNode->memberList) {
        if (member.id == id && member.port == port){
            return &member;
        }
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

	// update my heartbeat 
    if (!memberNode->bFailed) {
        memberNode->heartbeat ++;
        auto entry = check_member_list(&memberNode->addr);
        if(entry) {
            entry->heartbeat = memberNode->heartbeat;
            entry->timestamp = par->getcurrtime();
        }
    } else {
        for(int i=0; i<memberNode->memberList.size(); i++) {
            if(get_address(memberNode->memberList[i]) == memberNode->addr) {
                memberNode->memberList.erase(memberNode->memberList.begin() + i);
                break;
            }
        }
    }
    
    // delete old member
       for (int i = memberNode->memberList.size()-1 ; i >= 0; i--) {
        if(par->getcurrtime() - memberNode->memberList[i].timestamp >= TREMOVE) {
            Address removed_addr = get_address(memberNode->memberList[i]);
            log->logNodeRemove(&memberNode->addr, &removed_addr);
            memberNode->memberList.erase(memberNode->memberList.begin()+i);
        }
    }
    // propagate my membership list by sending ping msg
    if (!memberNode->bFailed) {
        
        MessageHdr* pingMsg = new MessageHdr();
        pingMsg->msgType = PING;
        pingMsg->members = memberNode->memberList;
        pingMsg->addr = &memberNode->addr;
        
        for(const auto& member: memberNode->memberList)  {
            // send Ping msg
            Address receiveAddress = Address(to_string(member.id) + ":" + to_string(member.port));
            emulNet->ENsend(&memberNode->addr, &receiveAddress, (char *)pingMsg, sizeof(MessageHdr));
        }
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
    printf("%d.%d.%d.%d:%d \n",  addr->addr[0],addr->addr[1],addr->addr[2],
                                                       addr->addr[3], *(short*)&addr->addr[4]) ;    
}

MemberListEntry* MP1Node::check_member_list(Address* node_addr) {
    for (size_t i = 0; i < memberNode->memberList.size(); i++) {
        int id = 0;
        short port = 0;
        memcpy(&id, &node_addr->addr[0], sizeof(int));
        memcpy(&port, &node_addr->addr[4], sizeof(short));
        if (memberNode->memberList[i].id == id && memberNode->memberList[i].port == port)
            return &memberNode->memberList[i];
    }
    return nullptr;
}

Address MP1Node::get_address(const MemberListEntry& entry) {
    return Address(to_string(entry.id) + ":" + to_string(entry.port));
}