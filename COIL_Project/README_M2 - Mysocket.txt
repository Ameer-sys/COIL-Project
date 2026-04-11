CSCN72050 - COIL Project Milestone 2
Group: Group-15 - CRC Syndicate
Members: Ishaq Ishaq Nasiru
         Shumroz Usmani
         Vineet Yogeshkumar Vaidya 
         Aryan Pramod Passi 

-- BEFORE BUILDING - VERIFY PROJECT SETTINGS
Two projects are in this solution.

  1. Right-click "PktDef" (COIL_Project_M1_Pktdef) project -> Properties
     General -> Configuration Type
     Must be set to: Static Library (.lib)
     If not, change it -> Apply -> OK

  2. Right-click "Mysocket" (Coil_Project_M2_Mysocket) project -> Properties
     General -> Configuration Type
     Must be set to: Static Library (.lib)
     If not, change it -> Apply -> OK

  3. Right-click "PktDefTests" (COIL_Project_Tests) project -> Properties
     General -> Configuration Type
     Must be set to: Dynamic Library (.dll)
     If not, change it -> Apply -> OK


NOTE: PktDef_Tests from Milestone 1 is included directly in the
test project. MySocket.cpp and PktDef.cpp are both added
as existing items to the COIL_Project_Tests MSTest project.

-- BUILD
  Build Solution
  Expected result: 0 errors, 0 warnings


-- RUN TESTS
  Test -> Run All Tests (Ctrl+R, A)
  Expected result: 32 tests pass (MySocket)

NOTES
- MySocket embeds PktDef from Milestone 1
- Both MysocketTest.cpp and PktDefTest.cpp are
  in the same test project (Coil_Project_Tests)
- No additional input files required
- Tested on Visual Studio 2022