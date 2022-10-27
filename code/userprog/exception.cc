// exception.cc 
//	Entry point into the Nachos kernel from user programs.
//	There are two kinds of things that can cause control to
//	transfer back to here from user code:
//
//	syscall -- The user code explicitly requests to call a procedure
//	in the Nachos kernel.  Right now, the only function we support is
//	"Halt".
//
//	exceptions -- The user code does something that the CPU can't handle.
//	For instance, accessing memory that doesn't exist, arithmetic errors,
//	etc.  
//
//	Interrupts (which can also cause control to transfer from user
//	code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "main.h"
#include "syscall.h"
//#include "userkernel.h"


//----------------------------------------------------------------------
// ExceptionHandler
// 	Entry point into the Nachos kernel.  Called when a user program
//	is executing, and either does a syscall, or generates an addressing
//	or arithmetic exception.
//
// 	For system calls, the following is the calling convention:
//
// 	system call code -- r2
//		arg1 -- r4
//		arg2 -- r5
//		arg3 -- r6
//		arg4 -- r7
//
//	The result of the system call, if any, must be put back into r2. 
//
// And don't forget to increment the pc before returning. (Or else you'll
// loop making the same system call forever!
//
//	"which" is the kind of exception.  The list of possible exceptions 
//	are in machine.h.
//----------------------------------------------------------------------
int count = 0;
void
ExceptionHandler(ExceptionType which)
{
	int	type = kernel->machine->ReadRegister(2);
	int	val, status, fileID,numChar;
    
    switch (which) {

	case PageFaultException: //hw3
      {    
       count++;
		 kernel->stats->numPageFaults = count;
        std::cout <<"Page fault exception no: "<<count<<"\n";
        unsigned int vpn;
        int vaddr = kernel->machine->ReadRegister(BadVAddrReg); //?
        vpn =  (unsigned) vaddr / PageSize;
        char *buff = new char[PageSize]; //?
        
        kernel->swapFile->ReadAt(buff, PageSize, kernel->currentThread->space->maptoSwap[vpn]*PageSize);        

        int physicalPageNo = kernel->bitmap->FindAndSet(); //?
        if (physicalPageNo != -1) //check if there is availability in main memory
        {
            //std::cout<<"memory avail \n";
            kernel->currentThread->space->setAttr(TRUE, physicalPageNo, vpn);            
            kernel->Phypage_ThreadMap.insert(pair<int, Thread*>(physicalPageNo, kernel->currentThread));
           
         //   kernel->stats->numPageFaults++;
            //bzero (&kernel->machine->mainMemory[physicalPageNo *PageSize], PageSize);
            bcopy (buff, &kernel->machine->mainMemory[physicalPageNo *PageSize], PageSize);
           // std::cout<<"page "<< count<<" copied to main memory \n";
        }
 
        else // main memory full. Choose page to swap from main memory (random page replacement algorithm)
        {
          //  std::cout<<"********** Main memory full. Swapping a page from main memory to swap file ********** \n";
            int randNo = rand()%128;
            char *buff1 = new char[PageSize];

            bcopy (&kernel->machine->mainMemory[randNo *PageSize], buff1, PageSize);
            kernel->bitmap->Clear(randNo);

            Thread *swapThread = kernel->Phypage_ThreadMap[randNo];
            
            int vpn_rep = swapThread->space->getVPN(randNo);
            if(vpn_rep!=-1)
            {
                int swapIndex = swapThread->space->maptoSwap[vpn_rep];
                kernel->swapFile->WriteAt(buff1, PageSize, swapIndex*PageSize);
                kernel->Phypage_ThreadMap.erase(randNo);
                
                int physicalPageNo = kernel->bitmap->FindAndSet(); 
                if (physicalPageNo != -1) //check if there is availability in main memory
                {  
                    //kernel->stats->numPageFaults+=1;
					kernel->currentThread->space->setAttr(TRUE, physicalPageNo, vpn);            
                    kernel->Phypage_ThreadMap.insert(pair<int, Thread*>(physicalPageNo, kernel->currentThread));
                    bcopy (buff, &kernel->machine->mainMemory[physicalPageNo *PageSize], PageSize);
                    //std::cout<<"page replaced \n";
                }
            }
            delete buff1;
        }
    delete buff;


		return;
    		
		ASSERTNOTREACHED();
        break;
	  }	  
	case SyscallException:
	    switch(type) {
		case SC_Halt:
		    DEBUG(dbgAddr, "Shutdown, initiated by user program.\n");
   		    kernel->interrupt->Halt();
		    break;
		case SC_PrintInt:
			val=kernel->machine->ReadRegister(4);
			cout << "Print integer:" <<val << endl;
			return;
    		
		    ASSERTNOTREACHED();
            break;
			//return;
/*		case SC_Exec:
			DEBUG(dbgAddr, "Exec\n");
			val = kernel->machine->ReadRegister(4);
			kernel->StringCopy(tmpStr, retVal, 1024);
			cout << "Exec: " << val << endl;
			val = kernel->Exec(val);
			kernel->machine->WriteRegister(2, val);
			return;
*/		case SC_Exit:
			DEBUG(dbgAddr, "Program exit\n");
			val=kernel->machine->ReadRegister(4);
			cout << "return value:" << val << endl;
			kernel->currentThread->Finish();
			return;
    		
		    ASSERTNOTREACHED();
			
			break;
       

		case SC_Msg:
		{
			//DEBUG(dbgSys, "Message received.\n");
			val = kernel->machine->ReadRegister(4);
			{
				char *msg = &(kernel->machine->mainMemory[val]);
				cout << msg << endl;
			}
			kernel->interrupt->Halt();
			ASSERTNOTREACHED();
			break;
		}

		case SC_Create:
			val = kernel->machine->ReadRegister(4);
			{
				char *filename = &(kernel->machine->mainMemory[val]);
				status = kernel->fileSystem->Create(filename);	
				kernel->machine->WriteRegister(2, (int)status);
			}
			return;
			ASSERTNOTREACHED();
			break;
		//<TODO
		
		case SC_Open:
		val = kernel->machine->ReadRegister(4); 
		{
		
    		//cout << "val = " << val << endl;
    		char *filename = &(kernel->machine->mainMemory[val]);
    		//cout << "filename = " << filename << endl;
    		status = kernel->fileSystem->OpenAFile(filename); 
    		kernel->machine->WriteRegister(2, (int) status);
		}	
    	    return;	
    		ASSERTNOTREACHED();
            break;
		
		case SC_Read:
		 val = kernel->machine->ReadRegister(4);

        {
              char *buffer = &(kernel->machine->mainMemory[val]);
              numChar = kernel->machine->ReadRegister(5);
              status = kernel->fileSystem->ReadFile(buffer,numChar);
              kernel->machine->WriteRegister(2, (int) status);          
        }

                    return;

                    ASSERTNOTREACHED();

                    break;

		case SC_Write:
	     val = kernel->machine->ReadRegister(4);

                    {
                        char *buffer = &(kernel->machine->mainMemory[val]);
                        numChar = kernel->machine->ReadRegister(5);
                        status =kernel->fileSystem->WriteFile1(buffer, numChar);
                        kernel->machine->WriteRegister(2, (int) status);
                    }


                    return;

                    ASSERTNOTREACHED();
	
		           break;
		case SC_Close:
        val = kernel->machine->ReadRegister(4);
	    
          {  
			 status = kernel->fileSystem->CloseFile();
             kernel->machine->WriteRegister(2, (int) status);
			// cout<<"hey!"<<endl;
		  }	
           
    		return;
    		
		ASSERTNOTREACHED();
        break;

        
		//TODO>
		default:
		    cerr << "Unexpected system call " << type << "\n";
 		    break;
		
	    }
	    break;
	default:
	    cerr << "Unexpected user mode exception" << which << "\n";
	    break;
    }
    ASSERTNOTREACHED();
}
