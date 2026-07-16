import os
cwd = "E:\\A NX FILE\\_Sourse\\以点选择移动与复制"
hpp = os.path.join(cwd, "以点选择移动与复制.hpp")

# Read raw bytes
with open(hpp, "rb") as f:
    raw = f.read()

print("File size:", len(raw), "bytes")
print("First 6 bytes (hex):", " ".join(f"{b:02x}" for b in raw[:6]))
print("Has UTF-8 BOM (EF BB BF):", raw[:3] == b"\xef\xbb\xbf")

# Check using different encoding
for enc in ["utf-8-sig", "utf-8", "gb2312", "gbk"]:
    try:
        content = raw.decode(enc)
        print(f"Can decode as {enc}: YES")
    except:
        print(f"Can decode as {enc}: NO")

# Try to decode with utf-8-sig and check contents
content = raw.decode("utf-8-sig")
print()
print("First 3 lines of header:")
for i, line in enumerate(content.split("\n")[:3]):
    print(f"  {i+1}: {repr(line.rstrip())}")

# Check that all needed includes are present
includes_to_check = [
    "BlockStyler_PropertyList.hxx",
    "BlockStyler_SpecifyPoint.hxx",
    "BlockStyler_SelectObject.hxx",
    "BlockStyler_Group.hxx",
    "BlockStyler_Enumeration.hxx",
    "BlockStyler_Button.hxx",
]
print()
for inc in includes_to_check:
    found = inc in content
    print(f"  {inc}: {'OK' if found else 'MISSING!'}")
