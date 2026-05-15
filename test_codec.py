import re
with open("Plugins/ForbocAI_SDK/Source/ForbocAI_SDK/Public/Protocol/ProtocolRequestTypes.h", "r") as f:
    header = f.read()

# find FNPCProcessTape
tape_match = re.search(r"struct FNPCProcessTape\s*\{(?P<body>.*?)\};", header, re.S)
if tape_match:
    print("Found Tape:")
    fields = re.findall(r"UPROPERTY.*?\n\s+(?:\w+|<.+>)\s+(\w+);", tape_match.group("body"))
    print(fields)

