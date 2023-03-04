

CC = gcc -g

All: leader followers

# leader: arpc_leader.cpp appendentries.h
# 	$(CC)  arpc_leader.cpp -o leader

# followers: arpc_followers.cpp appendentries.h
# 	$(CC) arpc_followers.cpp -o followers


leader: appendentriesRPC_leader.c appendentries.h
	$(CC)  appendentriesRPC_leader.c -o leader

followers: appendentriesRPC_followers.c appendentries.h
	$(CC) appendentriesRPC_followers.c -o followers

clean:
	rm -f leader followers

# $(CC) appendentriesRPC_leader.o appendentriesRPC_followers.o -o append