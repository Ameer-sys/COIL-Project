CSCN72050 - COIL Project Milestone 1
Group: Group-01 - CRC Syndicate
Members:Ishaq Ishaq Nasiru
        Shumroz Usmani
        Vineet Yogeshkumar Vaidya 
        Aryan Pramod Passi 

Date: 24th March 2026

-- BEFORE BUILDING - VERIFY PROJECT SETTINGS
Two project settings must be correct for a successful build.
If they are already set correctly, skip to the BUILD section.

  1. Right-click "PktDef" (COIL_Project) project -> Properties
     General -> Configuration Type
     Must be set to: Static Library (.lib)
     If not, change it -> Apply -> OK

  2. Right-click "PktDefTests" (COIL_Project_Tests) project -> Properties
     General -> Configuration Type
     Must be set to: Dynamic Library (.dll)
     If not, change it -> Apply -> OK

-- BUILD
  Build Solution
  Expected result: 0 errors, 0 warnings

-- RUN TESTS
  Test -> Run All Tests
  Expected result: 31 tests pass

NOTES
- No additional input files required
- No special execution instructions required
- Tested on Visual Studio 2022 - MSTEST framework
