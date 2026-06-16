# 801a4d90 
# right before update loop

    .include "Globals.s"
    
    backupall

    lis r3, 0x803d
    ori r3, r3, 0x709c
    mtctr r3

    bl Info_End
Info_Start:
Info_FnPtrLoc:   .long 0
Info_Filename:   .string "slp.dat";
.align 2
Info_RootSymbol: .string "fns";
.align 2
Info_End:
    mflr r30

    addi r3, r30, Info_Filename - Info_Start
    addi r4, r30, Info_FnPtrLoc - Info_Start
    addi r5, r30, Info_RootSymbol - Info_Start
    bctrl

    lwz r3, 0x0(r30)
    mtctr r3

    bctrl

Exit:
    restoreall
    addi	r28, r4, 12
