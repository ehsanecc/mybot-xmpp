mybot-xmpp
==========

This project's goal is to provide a *simple* but *powerful* robot for XMPP protocol.

in order to compile this project, you need to install **libstrophe**.


to compile the project, run:
```bash
./compile.sh
```
in bash


**responser**

responser is a part of mybot-xmpp that tries to response the incoming message
as much as human like. this module is need to grow larger for better answers
and much smarter.

responser requires a file with the following format, '#' flag uses for comments, so you can put this as a header of your response file.
here is an example of a simple response file, when someone joins the room it uses 
"hi" or "hello" or "welcome" randomaticaly. ( the use of '|' character to say "or" )
and when someone said "hi" or "hello"(perl regex) robot will answer "hello".
```
##############################################
# input format :
# q question regex; r response
# "q":{"r"|"r"|...}
# pre-compiled strings:
#     $id 	  id of bot (can be used in question string)
#	  $uid	  id of addressed person(who bot talking to) or user id
#	  $room	  room name (can be used in question string)
#	  $time	  get's current time(%H:%M:%S)
#	  $day	  get current's day time
#	  $bash   execute a bash command and get result replace(i.e $bash{ls}) or 
#                  it can only run a command silencely, i.e $bash[ls]
#     $1-9	  regex substrings
#
#	Options:
#	!private (default)
#   !admin
#	!public
#	!case-insensitive (default)
#	!case-sensitive
#	!welcome		  ; all flags will reset to default after first line of this flag
##############################################
! welcome
"":{"hi"|"hello"|"welcome"}

"^(hi|hello)$":{"hello"}
```
it is the most basic example of response file, you can use pre-compiled strings, $bash flag to run bash commands get from user command(use with caution) as follows:
```
!admin
"exe (.*)":"$bash{$1}"
```
as this will execute every command after "exe" string(only when bot admin says), like "exe ls".
