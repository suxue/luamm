local fs = require("fs")
local a = fs.path("/usr/lib/sys-root/mingw/bin/SDL2.dll")
print (a:filename())
print (a:parent_path())
print (a:parent_path():parent_path())
print (a:relative_path())
print (a:extension())
print (a:empty())
local b = a:extension()
print(a .. b)
print(b:is_absolute())
print(b < a)
print(b > a)

for part in a:each() do
   print(part)
end

for part in a:each(true) do
   print(part)
end
