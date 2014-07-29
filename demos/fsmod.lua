local fs = require("fs")
local a = fs.path("/")
a:filename()
a:parent_path()
a:parent_path():parent_path()
a:relative_path()
a:extension()
a:empty()
local b = a:extension()
local c

c = a .. b
c = b:is_absolute()
c = b < a
c = b > a

for part in a:each() do
   c = part
end

for part in a:each(true) do
   c = part
end

c = fs.path("/"):stat():type()
