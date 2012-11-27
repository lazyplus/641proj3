##################################
# 15641 proj3 readme             #
# by hanshil & ysu1              #
##################################
[TOC] Table of Content
----------------------------------
	[TOC] Table of Content
	[DES] Description of files
	[RUN] How to Run

[DES]
----------------------------------
	Our peer main consist of 3 components: a "requestor" which sends out "WHO HAS" messages and manages the downloading of the file chunks; a "responder" which sends out "I HAVE" message and prepare the file chunks; a "sender" which sends the file chunks out and does the congestion control.

	Makefile            make file for all source code
        readme.txt	    this file
        peer.c		    main() source file
        peer.h		    header
        requestor.c	    requestor source file
	requestor.h         requestor header file
	responser.c	    responser source file
	responser.h	    responser header file
	send.c		    sender source file
	send.h		    sender header file
	/test		    files for testing
	/config             files for configuration

[RUN]
----------------------------------
peer -p <peer-list-file> -c <has-chunk-file> -m <max-downloads> -i <peer-identity> -f <master-chunk-file> -d <debug-level>
