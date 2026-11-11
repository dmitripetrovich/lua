-- $Id: testes/lglm.lua $
-- See Copyright Notice in file all.lua

print("testing vectors, quaternions and matrices")

local COMPAT = rawget(_G, "vector3") ~= nil

local function check (msg, f, ...)
  local ok, err = pcall(f, ...)
  assert(not ok, "operation did not fail")
  assert(string.find(err, msg, 1, true), err)
end

local function approx (a, b)
  return all(glm.approx(a, b, 1e-5))
end

-- constructors and types
do
  assert(vec2(1, 2) == vec(1, 2))
  assert(vec3(1, 2, 3) == vec(1, 2, 3))
  assert(vec4(1, 2, 3, 4) == vec(1, 2, 3, 4))
  assert(vec3() == vec3(0, 0, 0))
  assert(vec3(7) == vec3(7, 7, 7))
  assert(vec3(vec2(1, 2), 3) == vec3(1, 2, 3))
  assert(vec4(1, vec3(2, 3, 4)) == vec4(1, 2, 3, 4))
  assert(vec({1, 2, 3}) == vec3(1, 2, 3))
  assert(vec3(true, false, nil) == vec3(1, 0, 0))
  assert(ivec3(1.7, -1.7, 2.2) == vec3(1, -1, 2))
  assert(bvec3(0, 2, -1) == vec3(0, 1, 1))
  check("unexpected", vec3, 1, 2)
  assert(vec(1) == vec3(1, 1, 1))
  check("unexpected", vec3, {}, {}, print)

  assert(quat() == quat(1, 0, 0, 0))
  assert(quat(vec3(1, 2, 3), 4) == quat(4, 1, 2, 3))
  assert(quat(vec4(1, 2, 3, 4)) == quat(4, 1, 2, 3))
  assert(math.type(vec3(1, 2, 3).x) == "float")

  local t2 = COMPAT and "vector2" or "vec2"
  local t3 = COMPAT and "vector3" or "vec3"
  local t4 = COMPAT and "vector4" or "vec4"
  assert(type(vec2()) == t2 and type(vec3()) == t3 and type(vec4()) == t4)
  assert(type(quat()) == "quat")
  assert(type(mat2()) == (COMPAT and "matrix" or "mat2x2"))
  assert(type(mat3()) == (COMPAT and "matrix" or "mat3x3"))
  assert(type(mat4()) == (COMPAT and "matrix" or "mat4x4"))
  assert(math.type(vec3()) == nil)
end

-- string conversion
do
  assert(tostring(vec2(1, 2.5)) == "vec2(1.000000, 2.500000)")
  assert(tostring(vec3(1, 2.5, -3)) == "vec3(1.000000, 2.500000, -3.000000)")
  assert(tostring(vec4(1, 2, 3, 4)) == "vec4(1.000000, 2.000000, 3.000000, 4.000000)")
  assert(tostring(quat(1, 0, 0, 0)) == "quat(1.000000, {0.000000, 0.000000, 0.000000})")
  assert(tostring(mat2()) == "mat2x2((1.000000, 0.000000), (0.000000, 1.000000))")
  assert(string.format("%s", vec2(1, 2)) == "vec2(1.000000, 2.000000)")
  if COMPAT then  -- grit-lua: vectors coerce to strings
    assert("v=" .. vec2(1, 2) == "v=vec2(1.000000, 2.000000)")
  else  -- concatenation appends components
    assert(vec2(1, 2) .. 3 == vec3(1, 2, 3))
    assert(vec2(1, 2) .. vec2(3, 4) == vec4(1, 2, 3, 4))
  end
end

-- field access
do
  local v = vec4(1, 2, 3, 4)
  assert(v.x == 1 and v.y == 2 and v.z == 3 and v.w == 4)
  assert(v.r == 1 and v.g == 2 and v.b == 3 and v.a == 4)
  assert(v.X == 1 and v.W == 4)
  assert(v[1] == 1 and v[4] == 4 and v[5] == nil and v[0] == nil)
  assert(v.n == 4 and vec2().n == 2)
  assert(v.zyx == vec3(3, 2, 1) and v.xy == vec2(1, 2))
  assert(v.wzyx == vec4(4, 3, 2, 1))
  assert(vec3(1, 2, 3).w == nil and v.foo == nil)
  local k = "x"
  assert(v[k] == 1 and v["yx"] == vec2(2, 1))
  assert(rawget(v, 2) == 2 and rawget(v, "z") == 3 and rawget(v, "zz") == nil)
  assert(#vec2(3, 4) == 5.0 and rawlen(vec2(3, 4)) == 2 and rawlen(quat()) == 4)
  local q = quat(4, 1, 2, 3)
  assert(q.x == 1 and q.y == 2 and q.z == 3 and q.w == 4 and q[4] == 4)
  check("modify", function () v.x = 5 end)
  check("modify", function () v[1] = 5 end)
end

-- iteration and unpacking
do
  local v = vec3(1, 2, 3)
  local s = ""
  for k, x in pairs(v) do s = s .. k .. "=" .. x .. " " end
  assert(s == "1=1.0 2=2.0 3=3.0 ")
  s = ""
  for k, x in ipairs(v) do s = s .. k .. "=" .. x .. " " end
  assert(s == "1=1.0 2=2.0 3=3.0 ")
  assert(next(v) == 1 and next(v, 3) == nil)
  assert(select("#", table.unpack(v)) == 3)
  local a, b, c = table.unpack(vec3(4, 5, 6))
  assert(a == 4 and b == 5 and c == 6)
  local w, x, y, z = table.unpack(quat(4, 1, 2, 3))
  assert(w == 4 and x == 1 and y == 2 and z == 3)
  if COMPAT then
    a, b, c = vec3(7, 8, 9)()
    assert(a == 7 and b == 8 and c == 9)
  end
end

-- arithmetic
do
  local v = vec3(1, 2, 3)
  assert(v + 1 == vec3(2, 3, 4) and 1 + v == vec3(2, 3, 4))
  assert(v - 1 == vec3(0, 1, 2) and 1 - v == vec3(0, -1, -2))
  assert(v * 2 == vec3(2, 4, 6) and 2 * v == vec3(2, 4, 6))
  assert(v * v == vec3(1, 4, 9) and v / v == vec3(1, 1, 1))
  assert(v / 2 == vec3(0.5, 1, 1.5) and 6 / v == vec3(6, 3, 2))
  assert(vec3(1.5, 2.5, -3.5) // 1 == vec3(1, 2, -4))
  assert(vec3(5, 6, 7) % 4 == vec3(1, 2, 3))
  assert(v ^ 2 == vec3(1, 4, 9))
  assert(-v == vec3(-1, -2, -3))
  assert(vec3(5, 6, 7) & 3 == vec3(1, 2, 3))
  assert(vec3(5, 6, 7) | 8 == vec3(13, 14, 15))
  assert(vec3(5, 6, 7) ~ 1 == vec3(4, 7, 6))
  assert(~vec3(1, 2, 3) == vec3(-2, -3, -4))
  assert(vec3(1, 2, 4) << 1 == vec3(2, 4, 8))
  assert(vec3(8, 8, 8) >> 2 == vec3(2, 2, 2))
  local three = 3
  assert(three & vec3(5, 6, 7) == vec3(1, 2, 3))
  assert(1 << vec3(1, 2, 3) == vec3(2, 4, 8))
  assert(mat2(2) * 2 == mat2(4) and 2 * mat2(2) == mat2(4) and -mat2(1) == mat2(-1))
  assert(tostring(v / 0) == "vec3(inf, inf, inf)")
  check("attempt to add a", function () return vec3() + vec4() end)
  check("attempt to compare", function () return vec3() < vec3() end)
  local x = vec2(1, 1)
  x += vec2(1, 2)
  assert(x == vec2(2, 3))
end

-- equality and table keys
do
  assert(vec3(1, 2, 3) == vec3(1, 2, 3) and vec3(1, 2, 3) ~= vec3(1, 2, 4))
  assert(vec3(1, 2, 3) ~= vec4(1, 2, 3, 0) and vec4(1, 2, 3, 4) ~= quat(4, 1, 2, 3))
  local t = {}
  t[vec3(1, 2, 3)] = "a"
  t[vec2(1, 2)] = "b"
  assert(t[vec3(1, 2, 3)] == "a" and t[vec2(1, 2)] == "b" and t[vec3(1, 2, 0)] == nil)
  check("NaN", function () t[vec2(0/0, 1)] = 1 end)
end

-- quaternions
do
  local deg = COMPAT and 1 or math.pi / 180
  local q = quat(90 * deg, vec3(0, 0, 1))
  assert(approx(q * vec3(1, 0, 0), vec3(0, 1, 0)))
  assert(approx(vec3(0, 1, 0) / q, vec3(1, 0, 0)))
  assert(approx(q * quat(-90 * deg, vec3(0, 0, 1)), quat()))
  assert(approx(quat(vec3(1, 0, 0), vec3(0, 1, 0)), q))
  assert(approx(inverse(q) * (q * vec3(1, 2, 3)), vec3(1, 2, 3)))
  assert(math.abs(glm.angle(q) - math.pi / 2) < 1e-5)
  assert(approx(glm.axis(q), vec3(0, 0, 1)))
  assert(approx(glm.slerp(quat(), q, 0.5), quat(45 * deg, vec3(0, 0, 1))))
end

-- matrices
do
  local m = mat3()
  assert(#m == 3 and rawlen(m) == 3 and m[1] == vec3(1, 0, 0) and m[4] == nil)
  m[2] = vec3(4, 5, 6)
  assert(m[2] == vec3(4, 5, 6))
  check("modify", function () m[2] = vec2(1, 2) end)
  check("modify", function () m[4] = vec3() end)
  check("invalid matrix rawset", rawset, m, 1, vec2())
  rawset(m, 2, vec3(0, 1, 0))
  assert(m == mat3())
  local r = mat(vec3(1, 0, 0), vec3(0, 0, 1), vec3(0, -1, 0))
  assert(r * vec3(0, 1, 0) == vec3(0, 0, 1))
  assert(r * mat3() == r and transpose(r) * r == mat3())
  assert(determinant(r) == 1 and inverse(r) == transpose(r))
  assert(mat2(2) == mat(vec2(2, 0), vec2(0, 2)))
  assert(type(mat()) == type(mat4()) and mat() == mat4())
  local s = mat3(); local c = mat(s)
  c[1] = vec3(9)
  assert(s == mat3() and c ~= s)
  local sink = mat3()
  assert(rawequal(mat3x3(sink, r), sink) and sink == r)
  local n = 0
  for k, col in pairs(r) do n = n + 1; assert(col == r[k]) end
  assert(n == 3)
  local a, b, cc = table.unpack(r)
  assert(a == r[1] and b == r[2] and cc == r[3])
  assert(mat2(vec2(1, 2)) == mat2(vec2(1, 0), vec2(0, 2)))
  local t = glm.translate(mat4(), vec3(1, 2, 3))
  assert(t * vec4(0, 0, 0, 1) == vec4(1, 2, 3, 1))
  local tr, rot, sc = glm.decompose(glm.compose(vec3(1, 2, 3), quat(), vec3(2, 2, 2)))
  assert(approx(tr, vec3(1, 2, 3)) and approx(rot, quat()) and approx(sc, vec3(2, 2, 2)))
  for i = 1, 100 do local _ = mat4() * mat4() end
  collectgarbage()
end

-- GLSL built-ins
do
  assert(dot(vec3(1, 2, 3), vec3(4, 5, 6)) == 32)
  assert(cross(vec3(1, 0, 0), vec3(0, 1, 0)) == vec3(0, 0, 1))
  assert(approx(normalize(vec3(3, 4, 0)), vec3(0.6, 0.8, 0)))
  assert(length(vec2(3, 4)) == 5 and distance(vec2(0, 0), vec2(3, 4)) == 5)
  assert(clamp(vec3(-1, 0.5, 2), 0, 1) == vec3(0, 0.5, 1))
  assert(clamp(5, 0, 3) == 3 and math.type(clamp(5, 0, 3)) == "integer")
  assert(mix(vec3(0), vec3(10), 0.25) == vec3(2.5))
  assert(min(vec2(1, 5), vec2(3, 2)) == vec2(1, 2) and max(1, 2) == 2)
  assert(abs(vec2(-1, 2)) == vec2(1, 2) and floor(vec2(1.5, -1.5)) == vec2(1, -2))
  assert(math.type(floor(1.5)) == "integer")
  assert(fract(vec2(1.25, 2.5)) == vec2(0.25, 0.5))
  assert(step(1, vec3(0, 1, 2)) == vec3(0, 1, 1))
  assert(lessThan(vec3(1, 2, 3), vec3(2, 2, 2)) == vec3(1, 0, 0))
  assert(equal(vec2(1, 2), vec2(1, 3)) == vec2(1, 0))
  assert(any(vec3(0, 0, 1)) and not all(vec3(0, 0, 1)))
  assert(not_(vec2(0, 1)) == vec2(1, 0))
  assert(radians(180) == math.pi and degrees(math.pi) == 180)
  assert(bitCount(7) == 3 and findLSB(8) == 3 and findMSB(8) == 3)
  assert(bitfieldExtract(0xF0, 4, 4) == 0xF)
  assert(glm.approx(1.0, 1.0 + 1e-9) == true)
  assert(glm.lerp == mix and glm.norm == normalize and glm.inv == inverse)
  check("unexpected", dot, vec2(), vec3())
end

-- library layout
do
  assert(type(glm) == "table" and package.loaded.glm == glm)
  assert(glm.epsilon > 0 and glm.epsilon < 1e-6)
  assert(glm.vec3 == nil and glm.dot == nil)
  for _, name in ipairs{"approx", "angle", "axis", "quat_for", "translate", "scale",
                        "rotate", "frustum", "ortho", "perspective", "project",
                        "unproject", "lookat", "billboard", "compose",
                        "decompose", "from_euler", "to_euler", "lerp", "epsilon"} do
    assert(glm[name] ~= nil and rawget(_G, name) == nil, name)
  end
  for _, name in ipairs{"inv", "norm", "slerp"} do
    assert(glm[name] ~= nil and (rawget(_G, name) ~= nil) == COMPAT, name)
  end
  if COMPAT then
    assert(vector3(1, 2, 3) == vec3(1, 2, 3) and vector(1, 2) == vec2(1, 2))
    assert(math.clamp(5, 0, 3) == 3.0 and math.type(math.clamp(5, 0, 3)) == "float")
  end
end

-- per-type metatables
do
  local old = debug.getmetatable(vec3())
  debug.setmetatable(vec3(), {__index = function (v, k) return k end, __call = function (v, a) return a end})
  assert(vec3().zz == vec2(0, 0) and vec3().foo == "foo" and vec2().foo == "foo")
  assert(vec3()(42) == 42)
  debug.setmetatable(vec3(), old)
  assert(vec3().foo == nil)
end

-- hashing
if joaat then
  assert(joaat("hello") == -922937317 and `hello` == -922937317)
  assert(`hello` == `HELLO` and joaat("Hello", true) ~= joaat("hello", true))
end

-- C API
if T then
  local a, b, c, d = T.testC("isvec2 2; isvec3 2; isvec4 2; isquat 2; return 4", vec3())
  assert(a == false and b == true and c == false and d == false)
  assert(T.testC("isvector 2; return 1", vec4()) == 4)
  assert(T.testC("isvector 2; return 1", 1.5) == 1)
  assert(T.testC("isvector 2; return 1", quat()) == 0)
  assert(T.testC("ismatrix 2; return 1", mat3()) == 3)
  assert(T.testC("pushvec2 1 2; return 1") == vec2(1, 2))
  assert(T.testC("pushvec3 1 2 3; return 1") == vec3(1, 2, 3))
  assert(T.testC("pushvec4 1 2 3 4; return 1") == vec4(1, 2, 3, 4))
  assert(T.testC("pushquat 4 1 2 3; return 1") == quat(4, 1, 2, 3))
  assert(T.testC("pushvector 1 2 3 4 3; return 1") == vec3(1, 2, 3))
  a, b, c = T.testC("checkvec3 2; return 3", vec3(5, 6, 7))
  assert(a == 5 and b == 6 and c == 7)
  local w, x, y, z = T.testC("checkquat 2; return 4", quat(4, 1, 2, 3))
  assert(w == 4 and x == 1 and y == 2 and z == 3)
  a, b = T.testC("tovector 2; return 2", vec2(1, 2))
  assert(a == 1 and b == 2)
  check("vec3 expected", T.testC, "checkvec3 2; return 0", 1)
  local t = vec3(1, 2, 3)
  assert(T.testC("gettable 2; return 1", t, 2) == 2)
  assert(T.testC("gettable 2; return 1", t, "zyx") == vec3(3, 2, 1))
  assert(T.testC("getfield 2 y; return 1", t) == 2)
  assert(T.testC("getfield 2 zyx; return 1", t) == vec3(3, 2, 1))
  assert(T.testC("rawgeti 2 1; return 1", t) == 1)
  assert(T.testC("objsize 2; return 1", t) == 3)
  local m = mat2()
  T.testC("pushnum 1; pushvalue 3; settable 2; return 0", m, vec2(5, 6))
  assert(m[1] == vec2(5, 6))
  T.testC("pushvalue 3; seti 2 2; return 0", m, vec2(7, 8))
  assert(m[2] == vec2(7, 8))
end

print("OK")
