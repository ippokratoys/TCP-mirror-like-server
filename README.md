( The recursive mkdir is from http://nion.modprobe.de/blog/archives/357-Recursive-directory-creation.html )
Ston content server h atnistoixia ginetai me ena id (int) to opoio exei diarkeia zwhs oso kai o content server,
dhladh an erthei kati me idio ID tha isxuei to delay tou paliou.

Ta thread tupou content server stenloun aitima list, pernoun thn apanthsh kai na einai subfolder tou zhtoumenou to krataei,
dhmiourgei tous aprethtous fakelous, alliws sunexizei me to epomeno.

Oi workers peroumenoun na erthei kati sto buffer, to diabazei, stelnei fetch gia auto, anoigei to arxeio, to grafei kai sunexizei.
Termatizoun otan termatisoun oloi oi mirror manager stelnontas nhma sto main thread to opoio perimenei na teliwsoun oloi oi mirror manager.

To main thread dhmimourgei workers, diabazei apo to socket me ton initiator, dhmmiourgei tous manager kai meta perimenei na teliwseoun ola ta alla thread. Otan ginei auto upologozei ta pososta kai ta stelnei pisw ston initiator(exontas diatirisei to socket anoixto
