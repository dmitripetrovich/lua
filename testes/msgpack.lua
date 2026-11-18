print("testing msgpack")

local json = require "json"
local msgpack = require "msgpack"

local function fails (f, ...)
  return not pcall(f, ...)
end

local function deepeq (a, b)
  if type(a) ~= type(b) then return false end
  if type(a) ~= "table" then
    if a ~= a and b ~= b then return true end
    return a == b and math.type(a) == math.type(b)
  end
  for k, v in pairs(a) do if not deepeq(v, b[k]) then return false end end
  for k in pairs(b) do if a[k] == nil then return false end end
  return true
end


print("testing msgpack round trips")

for _, v in ipairs({ 0, 1, -1, 127, 128, -32, -33, 2^40 | 0, -(2^40 | 0), math.maxinteger, math.mininteger, 1.5, -0.25, 1e300, "", "abc", string.rep("z", 70000), true, false }) do
  assert(deepeq(msgpack.unpack(msgpack.pack(v)), v))
end

do
  local nested = { a = { 1, 2, { b = "c" } }, [10] = false, list = { 1.5, "x", {} } }
  assert(deepeq(msgpack.unpack(msgpack.pack(nested)), nested))
  local big = {}
  for i = 1, 1000 do big["k" .. i] = i end
  assert(deepeq(msgpack.unpack(msgpack.pack(big)), big))
  local a, b, c = msgpack.unpack(msgpack.pack(1, "two", { 3 }))
  assert(a == 1 and b == "two" and deepeq(c, { 3 }))
end

assert(msgpack.getoption("small_lua") == false and msgpack.getoption("full64bits") == true)


print("testing msgpack packers")

do
  local p = msgpack.new()
  p:array({ 1, 2 }, { 3 })
  local a1, a2 = msgpack.unpack(tostring(p))
  assert(deepeq(a1, { 1, 2 }) and deepeq(a2, { 3 }))
  p = msgpack.new()
  p:integer(2^40 | 0)
  assert(msgpack.unpack(tostring(p)) == 2^40 | 0)
  assert(type(msgpack.packers.int) == "function")
  assert(fails(p.uint8, p, "x"))
  assert(fails(p.uint8, p, 1.5))
  assert(fails(p.binary, p, {}))
  assert(fails(p.float, p, "y"))
  p = msgpack.new()
  p:string(12)
  assert(msgpack.unpack(tostring(p)) == "12")
  assert(fails(function () return p .. {} end))
end


print("testing msgpack options")

msgpack.setoption("integer", true)
assert(msgpack.unpack(msgpack.pack(1.5)) == 1.5)
assert(math.type(msgpack.unpack(msgpack.pack(3.0))) == "integer")
msgpack.setoption("double", true)
assert(math.type(msgpack.unpack(msgpack.pack(3.0))) == "float")

msgpack.setoption("with_hole", true)
do
  local clock = os.clock()
  local bomb = msgpack.unpack(msgpack.pack({ n = 1e12 | 0 }))
  assert(os.clock() - clock < 1 and bomb.n == 1e12 | 0)
  assert(#msgpack.pack({ n = 3 }) == 4)
  assert(deepeq(msgpack.unpack(msgpack.pack({ 1, 2, n = 2 })), { 1, 2 }))
end
msgpack.setoption("without_hole", true)

assert(fails(msgpack.setoption, "full64bits", true))
assert(fails(msgpack.setoption, "with_hole", 1))
assert(fails(msgpack.setoption, "bogus", true))


print("testing msgpack.with")

do
  local mp = msgpack.with({ with_hole = true, sentinel = true })
  assert(mp.pack({ 1, nil, 3 }) == "\x93\x01\xc0\x03")
  assert(msgpack.pack({ 1, nil, 3 }) ~= mp.pack({ 1, nil, 3 }))
  assert(mp.unpack("\x91\xc0")[1] == msgpack.null and msgpack.unpack("\x91\xc0")[1] == nil)
  msgpack.setoption("always_as_map", true)
  assert(mp.pack({ 1, 2 }) == "\x92\x01\x02")
  msgpack.setoption("without_hole", true)
  assert(mp.pack_args(1, nil) == "\x92\x01\xc0")
  assert(select(2, mp.next(mp.pack(5))) == 5)
  local bound = mp.new()
  bound({ 1, nil, 3 })
  assert(tostring(bound) == "\x93\x01\xc0\x03")
  assert(mp.null == msgpack.null)
  assert(msgpack.with({}).pack({ 1, nil, 3 }) == "\x82\x01\x01\x03\x03")
  assert(fails(msgpack.with, { bogus = true }))
  assert(fails(msgpack.with, { with_hole = 1 }))
  assert(fails(msgpack.with, { true }))
  assert(fails(msgpack.with, { full64bits = true }))
end


print("testing msgpack decoding")

do
  local ok, dec = pcall(msgpack.unpack, "\x82\xcb\x7f\xf8\x00\x00\x00\x00\x00\x00\x01\xa1a\x02")
  assert(ok and dec.a == 2)
  assert(pcall(msgpack.unpack, string.rep("\x91", 32) .. "\x90"))
  assert(fails(msgpack.unpack, string.rep("\x91", 33) .. "\xc0"))
  assert(fails(msgpack.unpack, "\x92\x01"))
  assert(select("#", msgpack.next(msgpack.pack(1, 2), 1, 2^40 | 0)) >= 2)
  assert(fails(msgpack.next, msgpack.pack(1), 1, -1))
end


print("testing msgpack extensions")

do
  local Point = {}
  Point.__index = Point
  msgpack.extend({
    __ext = 5,
    __pack = function (v) return string.pack("<ii", v.x, v.y) end,
    __unpack = function (s)
      local x, y = string.unpack("<ii", s)
      return setmetatable({ x = x, y = y }, Point)
    end,
  })
  Point.__ext = 5
  local pt = msgpack.unpack(msgpack.pack(setmetatable({ x = 3, y = -4 }, Point)))
  assert(getmetatable(pt) == Point and pt.x == 3 and pt.y == -4)
  assert(fails(msgpack.settype, "table", 300))
  assert(fails(msgpack.extend_clear, 1, 20))
  msgpack.extend_clear(5)
  assert(deepeq(msgpack.unpack(msgpack.pack(vec3(1, 2, 3))), vec3(1, 2, 3)))
end


print("testing msgpack.null")

do
  assert(msgpack.null == json.null and msgpack.null == msgpack.sentinel)
  assert(msgpack.pack(json.null) == msgpack.pack(nil))
  json.setoption("null", false)
  local withnull = json.decode("[1,null,2]")
  assert(msgpack.pack(withnull) == "\x93\x01\xc0\x02")
  json.setoption("null", true)
  msgpack.setoption("sentinel", true)
  local back = msgpack.unpack("\x93\x01\xc0\x02")
  assert(back[2] == json.null and json.encode(back) == "[1,null,2]")
  msgpack.setoption("sentinel", false)
end

print("OK")
