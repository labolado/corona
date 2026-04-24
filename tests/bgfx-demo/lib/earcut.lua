---[[
local bit = require("plugin.bit")
local bnot = bit.bnot
local band, bor, bxor = bit.band, bit.bor, bit.bxor
local lshift, rshift, rol = bit.lshift, bit.rshift, bit.rol
local max, min = math.max, math.min

local Earcut = {}
local _push = _.push
local linkedList, eliminateHoles, earcutLinked
local filterPoints, isEar, isEarHashed
local cureLocalIntersections, splitEarcut, eliminateHoles, compareX, eliminateHole
local findHoleBridge, indexCurve, sortLinked, zOrder, getLeftmost, isValidDiagonal
local area, equals, intersects, intersectsPolygon, locallyInside
local middleInside, splitPolygon, insertNode, removeNode, Node, signedArea
local sectorContainsSector, onSegment, sign

function Earcut.process(data, holeIndices, dim)
	dim = dim or 2

	local hasHoles = holeIndices and #holeIndices
	local outerLen = hasHoles > 0 and holeIndices[1] * dim or #data
	local outerNode = linkedList(data, 0, outerLen, dim, true)
	local triangles = {}

    if (not outerNode or outerNode.next == outerNode.prev) then return triangles end

	local minX, minY, maxX, maxY, x, y, invSize

	if (hasHoles) then
		outerNode = eliminateHoles(data, holeIndices, outerNode, dim)
	end

	-- if the shape is not too simple, we'll use z-order curve hash later calculate polygon bbox
	if (#data > 80 * dim) then
	    minX = data[1]
	    maxX = minX
	    minY = data[2]
	    maxY = minY

	    -- for (local i = dim i < outerLen i += dim) {
	    for i = dim + 1, outerLen, dim do
	        x = data[i]
	        y = data[i + 1]
	        if (x < minX) then minX = x end
	        if (y < minY) then minY = y end
	        if (x > maxX) then maxX = x end
	        if (y > maxY) then maxY = y end
	    end

	    -- minX, minY and invSize are later used to transform coords into integers for z-order calculation
	    invSize = max(maxX - minX, maxY - minY)
	    invSize = invSize ~= 0 and 1 / invSize or 0
	end

	earcutLinked(outerNode, triangles, dim, minX, minY, invSize)

	return triangles
end

setmetatable(Earcut, {
	__call = function(_, data, holeIndices, dim)
		return Earcut.process(data, holeIndices, dim)
	end
})

-- create a circular doubly linked list from polygon points in the specified winding order
function linkedList(data, start, endp, dim, clockwise)
    -- local i, last
    local last

    if (clockwise == (signedArea(data, start, endp, dim) > 0)) then
        -- for (i = start; i < end; i += dim) last = insertNode(i, data[i], data[i + 1], last);
        for i = start + 1, endp, dim do
	        last = insertNode(i, data[i], data[i + 1], last)
	    end
    else
        -- for (i = end - dim; i >= start; i -= dim) last = insertNode(i, data[i], data[i + 1], last);
		for i = endp - dim + 1, start + 1, -dim do
			last = insertNode(i, data[i], data[i + 1], last)
		end
    end

    if (last and equals(last, last.next)) then
        removeNode(last)
        last = last.next
    end

    return last
end

-- eliminate colinear or duplicate points
function filterPoints(start, endp)
    if (not start) then return start end
    if (not endp) then endp = start end

    local p = start
    local again
    repeat
        again = false

        if ((not p.steiner) and (equals(p, p.next) or area(p.prev, p, p.next) == 0)) then
            removeNode(p)
            p = p.prev
            endp = p
            if (p == p.next) then break end
            again = true
        else
            p = p.next
        end
    until (not (again or p ~= endp))

    return endp
end

-- main ear slicing loop which triangulates a polygon (given as a linked list)
function earcutLinked(ear, triangles, dim, minX, minY, invSize, pass)
    if (not ear) then return end

    -- interlink polygon nodes in z-order
    if ((not pass) and invSize) then indexCurve(ear, minX, minY, invSize) end

    local stop = ear
    local prev, next

    -- iterate through ears, slicing them one by one
    while (ear.prev ~= ear.next) do
        prev = ear.prev
        next = ear.next

        if (invSize and isEarHashed(ear, minX, minY, invSize) or isEar(ear)) then
            -- cut off the triangle
            _push(triangles, (prev.i + dim - 1) / dim)
            _push(triangles, (ear.i + dim - 1) / dim)
            _push(triangles, (next.i + dim - 1) / dim)

            removeNode(ear)

            -- skipping the next vertex leads to less sliver triangles
            ear = next.next
            stop = next.next

            -- continue
        else
	        ear = next

	        -- if we looped through the whole remaining polygon and can't find any more ears
	        if (ear == stop) then
	            -- try filtering points and slicing again
	            if (not pass) then
	                earcutLinked(filterPoints(ear), triangles, dim, minX, minY, invSize, 1)

	            -- if this didn't work, try curing all small self-intersections locally
	            elseif (pass == 1) then
                    ear = cureLocalIntersections(filterPoints(ear), triangles, dim);
	                earcutLinked(ear, triangles, dim, minX, minY, invSize, 2)

	            -- as a last resort, try splitting the remaining polygon into two
	            elseif (pass == 2) then
	                splitEarcut(ear, triangles, dim, minX, minY, invSize)
	            end

	            break
	        end
        end
    end
end

-- check whether a polygon node forms a valid ear with adjacent nodes
function isEar(ear)
    local a = ear.prev
    local b = ear
    local c = ear.next

    if (area(a, b, c) >= 0) then return false end -- reflex, can't be an ear

    -- now make sure we don't have other points inside the potential ear
    local p = ear.next.next

    while (p ~= ear.prev) do
        if (pointInTriangle(a.x, a.y, b.x, b.y, c.x, c.y, p.x, p.y) and
            area(p.prev, p, p.next) >= 0) then
            return false
        end
        p = p.next
    end

    return true
end

function isEarHashed(ear, minX, minY, invSize)
    local a = ear.prev
    local b = ear
	local c = ear.next

    if (area(a, b, c) >= 0) then return false end -- reflex, can't be an ear

    -- triangle bbox min & max are calculated like this for speed
    local minTX = a.x < b.x and (a.x < c.x and a.x or c.x) or (b.x < c.x and b.x or c.x)
    local minTY = a.y < b.y and (a.y < c.y and a.y or c.y) or (b.y < c.y and b.y or c.y)
    local maxTX = a.x > b.x and (a.x > c.x and a.x or c.x) or (b.x > c.x and b.x or c.x)
    local maxTY = a.y > b.y and (a.y > c.y and a.y or c.y) or (b.y > c.y and b.y or c.y)

    -- z-order range for the current triangle bbox
    local minZ = zOrder(minTX, minTY, minX, minY, invSize)
	local maxZ = zOrder(maxTX, maxTY, minX, minY, invSize)

    local p = ear.prevZ
	local n = ear.nextZ

    -- look for points inside the triangle in both directions
    while (p and p.z >= minZ and n and n.z <= maxZ) do
        if (p ~= ear.prev and p ~= ear.next and
            pointInTriangle(a.x, a.y, b.x, b.y, c.x, c.y, p.x, p.y) and
            area(p.prev, p, p.next) >= 0) then
            return false
		end
        p = p.prevZ

        if (n ~= ear.prev and n ~= ear.next and
            pointInTriangle(a.x, a.y, b.x, b.y, c.x, c.y, n.x, n.y) and
            area(n.prev, n, n.next) >= 0) then
           	return false
        end
        n = n.nextZ
    end

    -- look for remaining points in decreasing z-order
    while (p and p.z >= minZ) do
        if (p ~= ear.prev and p ~= ear.next and
            pointInTriangle(a.x, a.y, b.x, b.y, c.x, c.y, p.x, p.y) and
            area(p.prev, p, p.next) >= 0) then
            return false
        end
        p = p.prevZ
    end

    -- look for remaining points in increasing z-order
    while (n and n.z <= maxZ) do
        if (n ~= ear.prev and n ~= ear.next and
            pointInTriangle(a.x, a.y, b.x, b.y, c.x, c.y, n.x, n.y) and
            area(n.prev, n, n.next) >= 0) then
            return false
        end
        n = n.nextZ
    end

    return true
end

-- go through all polygon nodes and cure small local self-intersections
function cureLocalIntersections(start, triangles, dim)
    local p = start
    repeat
        local a = p.prev
        local b = p.next.next

        if ((not equals(a, b)) and intersects(a, p, p.next, b) and locallyInside(a, b) and locallyInside(b, a)) then

            _push(triangles, (a.i + dim - 1) / dim)
            _push(triangles, (p.i + dim - 1) / dim)
            _push(triangles, (b.i + dim - 1) / dim)

            -- remove two nodes involved
            removeNode(p)
            removeNode(p.next)

            p = b
            start = p
        end
        p = p.next
    until (p == start)

    return filterPoints(p)
end

-- try splitting polygon into two and triangulate them independently
function splitEarcut(start, triangles, dim, minX, minY, invSize)
    -- look for a valid diagonal that divides the polygon into two
    local a = start
    repeat
        local b = a.next.next
        while (b ~= a.prev) do
            if (a.i ~= b.i and isValidDiagonal(a, b)) then
                -- split the polygon in two by the diagonal
                local c = splitPolygon(a, b)

                -- filter colinear points around the cuts
                a = filterPoints(a, a.next)
                c = filterPoints(c, c.next)

                -- run earcut on each half
                earcutLinked(a, triangles, dim, minX, minY, invSize)
                earcutLinked(c, triangles, dim, minX, minY, invSize)
                return
            end
            b = b.next
        end
        a = a.next
    until (a == start)
end

-- link every hole into the outer loop, producing a single-ring polygon without holes
function eliminateHoles(data, holeIndices, outerNode, dim)
    local queue = {}
    -- local i, len, start, endp, list
    local len, start, endp, list

    -- for (i = 0, len = holeIndices.length i < len i++) {
    local len = #holeIndices
    for i=1, len do
        start = holeIndices[i] * dim
        endp = i < len and holeIndices[i + 1] * dim or #data
        list = linkedList(data, start, endp, dim, false)
        if (list == list.next) then list.steiner = true end
        _push(queue, (getLeftmost(list)))
    end

    -- queue.sort(compareX)
    table.sort(queue, compareX)

    -- process holes from left to right
    -- for (i = 0 i < queue.length i++) {
   	for i = 1, #queue do
        eliminateHole(queue[i], outerNode)
        outerNode = filterPoints(outerNode, outerNode.next)
    end

    return outerNode
end

function compareX(a, b)
    -- return a.x - b.x
    return a.x < b.x
end

-- find a bridge between vertices that connects hole with an outer ring and and link it
function eliminateHole(hole, outerNode)
    outerNode = findHoleBridge(hole, outerNode)
    if (outerNode) then
        local b = splitPolygon(outerNode, hole)
        filterPoints(b, b.next)
    end
end

-- David Eberly's algorithm for finding a bridge between hole and outer polygon
function findHoleBridge(hole, outerNode)
    local p = outerNode
    local hx = hole.x
    local hy = hole.y
    local qx = -math.huge
    local m

    -- find a segment intersected by a ray from the hole's leftmost point to the left
    -- segment's endpoint with lesser x will be potential connection point
    repeat
        if (hy <= p.y and hy >= p.next.y and p.next.y ~= p.y) then
            local x = p.x + (hy - p.y) * (p.next.x - p.x) / (p.next.y - p.y)
            if (x <= hx and x > qx) then
                qx = x
                if (x == hx) then
                    if (hy == p.y) then return p end
                    if (hy == p.next.y) then return p.next end
                end
                m = p.x < p.next.x and p or p.next
            end
        end
        p = p.next
    until (p == outerNode)

    if (not m) then return nil end

    if (hx == qx) then return m end -- hole touches outer segment; pick leftmost endpoint

    -- look for points inside the triangle of hole point, segment intersection and endpoint
    -- if there are no points found, we have a valid connection
    -- otherwise choose the point of the minimum angle with the ray as connection point

    local stop = m
    local mx = m.x
    local my = m.y
    local tanMin = math.huge
    local tan

    p = m

    local abs = math.abs
    repeat
        if (hx >= p.x and p.x >= mx and hx ~= p.x and
                pointInTriangle(hy < my and hx or qx, hy, mx, my, hy < my and qx or hx, hy, p.x, p.y)) then

            tan = abs(hy - p.y) / (hx - p.x) -- tangential

            -- if ((tan < tanMin or (tan == tanMin and p.x > m.x)) and locallyInside(p, hole)) then
            if (locallyInside(p, hole) and
                (tan < tanMin or (tan == tanMin and (p.x > m.x or (p.x == m.x and sectorContainsSector(m, p)))))) then
                m = p
                tanMin = tan
            end
        end

        p = p.next
    until (p == stop)

    return m
end

-- whether sector in vertex m contains sector in vertex p in the same coordinates
function sectorContainsSector(m, p)
    return area(m.prev, m, p.prev) < 0 and area(p.next, m, m.next) < 0
end

-- interlink polygon nodes in z-order
function indexCurve(start, minX, minY, invSize)
    local p = start
    repeat
        if (p.z == nil) then p.z = zOrder(p.x, p.y, minX, minY, invSize) end
        p.prevZ = p.prev
        p.nextZ = p.next
        p = p.next
    until (p == start)

    p.prevZ.nextZ = nil
    p.prevZ = nil

    sortLinked(p)
end

-- Simon Tatham's linked list merge sort algorithm
-- http:--www.chiark.greenend.org.uk/~sgtatham/algorithms/listsort.html
function sortLinked(list)
    local i, p, q, e, tail, numMerges, pSize, qSize
    local inSize = 1

    repeat
        p = list
        list = nil
        tail = nil
        numMerges = 0

        while (p) do
            -- numMerges++
            numMerges = numMerges + 1
            q = p
            pSize = 0
            -- for (i = 0 i < inSize i++) {
            for i = 1, inSize do
                -- pSize++
                pSize = pSize + 1
                q = q.nextZ
                if (not q) then break end
            end
            qSize = inSize

            while (pSize > 0 or (qSize > 0 and q)) do

                if (pSize ~= 0 and (qSize == 0 or (not q) or p.z <= q.z)) then
                    e = p
                    p = p.nextZ
                    pSize = pSize - 1
                else
                    e = q
                    q = q.nextZ
                    qSize = qSize - 1
                end

                if (tail) then
                	tail.nextZ = e
                else
                	list = e
                end

                e.prevZ = tail
                tail = e
            end

            p = q
        end

        tail.nextZ = nil
        inSize = inSize * 2

    until (numMerges <= 1)

    return list
end

-- z-order of a point given coords and inverse of the longer side of data bbox
function zOrder(x, y, minX, minY, invSize)
    -- coords are transformed into non-negative 15-bit integer range
    x = 32767 * (x - minX) * invSize
    y = 32767 * (y - minY) * invSize

    -- x = (x | (x << 8)) & 0x00FF00FF
    -- x = (x | (x << 4)) & 0x0F0F0F0F
    -- x = (x | (x << 2)) & 0x33333333
    -- x = (x | (x << 1)) & 0x55555555
    x = band(bor(x, lshift(x, 8)), 0x00FF00FF)
    x = band(bor(x, lshift(x, 4)), 0x0F0F0F0F)
    x = band(bor(x, lshift(x, 2)), 0x33333333)
    x = band(bor(x, lshift(x, 1)), 0x55555555)

    -- y = (y | (y << 8)) & 0x00FF00FF
    -- y = (y | (y << 4)) & 0x0F0F0F0F
    -- y = (y | (y << 2)) & 0x33333333
    -- y = (y | (y << 1)) & 0x55555555
    y = band(bor(y, lshift(y, 8)), 0x00FF00FF)
    y = band(bor(y, lshift(y, 4)), 0x0F0F0F0F)
    y = band(bor(y, lshift(y, 2)), 0x33333333)
    y = band(bor(y, lshift(y, 1)), 0x55555555)

    -- return x | (y << 1)
    return bor(x, lshift(y, 1))
end

-- find the leftmost node of a polygon ring
function getLeftmost(start)
    local p = start
    local leftmost = start
    repeat
        -- if (p.x < leftmost.x) then leftmost = p end
        if (p.x < leftmost.x or (p.x == leftmost.x and p.y < leftmost.y)) then leftmost = p end
        p = p.next
    until (p == start)

    return leftmost
end

-- check if a point lies within a convex triangle
function pointInTriangle(ax, ay, bx, by, cx, cy, px, py)
    return (cx - px) * (ay - py) - (ax - px) * (cy - py) >= 0 and
           (ax - px) * (by - py) - (bx - px) * (ay - py) >= 0 and
           (bx - px) * (cy - py) - (cx - px) * (by - py) >= 0
end

-- check if a diagonal between two polygon nodes is valid (lies in polygon interior)
function isValidDiagonal(a, b)
    return (a.next.i ~= b.i) and (a.prev.i ~= b.i) and (not intersectsPolygon(a, b)) and -- dones't intersect other edges
           (locallyInside(a, b) and locallyInside(b, a) and middleInside(a, b) and -- locally visible
            (area(a.prev, a, b.prev) or area(a, b.prev, b)) or -- does not create opposite-facing sectors
            equals(a, b) and area(a.prev, a, a.next) > 0 and area(b.prev, b, b.next) > 0) -- special zero-length case
end

-- signed area of a triangle
function area(p, q, r)
    return (q.y - p.y) * (r.x - q.x) - (q.x - p.x) * (r.y - q.y)
end

-- check if two points are equal
function equals(p1, p2)
    return p1.x == p2.x and p1.y == p2.y
end

-- check if two segments intersect
function intersects(p1, q1, p2, q2)
    local o1 = sign(area(p1, q1, p2))
    local o2 = sign(area(p1, q1, q2))
    local o3 = sign(area(p2, q2, p1))
    local o4 = sign(area(p2, q2, q1))

    if (o1 ~= o2 and o3 ~= o4) then return true end -- general case

    if (o1 == 0 and onSegment(p1, p2, q1)) then return true end -- p1, q1 and p2 are collinear and p2 lies on p1q1
    if (o2 == 0 and onSegment(p1, q2, q1)) then return true end -- p1, q1 and q2 are collinear and q2 lies on p1q1
    if (o3 == 0 and onSegment(p2, p1, q2)) then return true end -- p2, q2 and p1 are collinear and p1 lies on p2q2
    if (o4 == 0 and onSegment(p2, q1, q2)) then return true end -- p2, q2 and q1 are collinear and q1 lies on p2q2

    return false
end

-- for collinear points p, q, r, check if point q lies on segment pr
function onSegment(p, q, r)
    return q.x <= max(p.x, r.x) and q.x >= min(p.x, r.x) and q.y <= max(p.y, r.y) and q.y >= min(p.y, r.y)
end

function sign(num)
    return num > 0 and 1 or (num < 0 and -1 or 0)
end

-- check if a polygon diagonal intersects any polygon segments
function intersectsPolygon(a, b)
    local p = a
    repeat
        if (p.i ~= a.i and p.next.i ~= a.i and p.i ~= b.i and p.next.i ~= b.i and
                intersects(p, p.next, a, b)) then
        	return true
        end
        p = p.next
    until (p == a)

    return false
end

-- check if a polygon diagonal is locally inside the polygon
function locallyInside(a, b)
    -- return area(a.prev, a, a.next) < 0 ?
    --     (area(a, b, a.next) >= 0 and area(a, a.prev, b) >= 0) :
    --     (area(a, b, a.prev) < 0 or area(a, a.next, b) < 0)
	if area(a.prev, a, a.next) < 0 then
		return (area(a, b, a.next) >= 0 and area(a, a.prev, b) >= 0)
	else
		return (area(a, b, a.prev) < 0 or area(a, a.next, b) < 0)
	end
end

-- check if the middle point of a polygon diagonal is inside the polygon
function middleInside(a, b)
    local p = a
    local inside = false
    local px = (a.x + b.x) / 2
    local py = (a.y + b.y) / 2
    repeat
        if (((p.y > py) ~= (p.next.y > py)) and p.next.y ~= p.y and
                (px < (p.next.x - p.x) * (py - p.y) / (p.next.y - p.y) + p.x)) then
            inside = not inside
        end
        p = p.next
    until (p == a)

    return inside
end

-- link two polygon vertices with a bridge if the vertices belong to the same ring, it splits polygon into two
-- if one belongs to the outer ring and another to a hole, it merges it into a single ring
function splitPolygon(a, b)
    local a2 = Node(a.i, a.x, a.y)
    local b2 = Node(b.i, b.x, b.y)
    local an = a.next
    local bp = b.prev

    a.next = b
    b.prev = a

    a2.next = an
    an.prev = a2

    b2.next = a2
    a2.prev = b2

    bp.next = b2
    b2.prev = bp

    return b2
end

-- create a node and optionally link it with previous one (in a circular doubly linked list)
function insertNode(i, x, y, last)
    -- local p = new Node(i, x, y)
    local p = Node(i, x, y)

    if (not last) then
        p.prev = p
        p.next = p

    else
        p.next = last.next
        p.prev = last
        last.next.prev = p
        last.next = p
    end
    return p
end

function removeNode(p)
    p.next.prev = p.prev
    p.prev.next = p.next

    if (p.prevZ) then p.prevZ.nextZ = p.nextZ end
    if (p.nextZ) then p.nextZ.prevZ = p.prevZ end
end

function Node(i, x, y)
	local this = {}
    -- vertex index in coordinates array
    this.i = i

    -- vertex coordinates
    this.x = x
    this.y = y

    -- previous and next vertex nodes in a polygon ring
    this.prev = nil
    this.next = nil

    -- z-order curve value
    this.z = nil

    -- previous and next nodes in z-order
    this.prevZ = nil
    this.nextZ = nil

    -- indicates whether this is a steiner point
    this.steiner = false
    return this
end

-- return a percentage difference between the polygon area and its triangulation area
-- used to verify correctness of triangulation
function Earcut.deviation(data, holeIndices, dim, triangles)
    local hasHoles = holeIndices and #holeIndices
    local outerLen = hasHoles and holeIndices[1] * dim or #data

    local abs = math.abs
    local polygonArea = abs(signedArea(data, 0, outerLen, dim))
    if (hasHoles) then
        -- for (local i = 0, len = holeIndices.length i < len i++) {
       	local len = #holeIndices
        for i=1, len do
            local start = holeIndices[i] * dim
            local endp = i < len and holeIndices[i + 1] * dim or #data
            polygonArea = polygonArea - abs(signedArea(data, start, endp, dim))
        end
    end

    local trianglesArea = 0
    -- for (i = 0 i < triangles.length i += 3) {
   	for i = 1, #triangles, 3 do
        local a = triangles[i] * dim
        local b = triangles[i + 1] * dim
        local c = triangles[i + 2] * dim
        trianglesArea = trianglesArea + abs(
            (data[a] - data[c]) * (data[b + 1] - data[a + 1]) -
            (data[a] - data[b]) * (data[c + 1] - data[a + 1]))
    end

    return (polygonArea == 0 and trianglesArea == 0) and 0 or
        abs((trianglesArea - polygonArea) / polygonArea)
end

function signedArea(data, start, endp, dim)
    local sum = 0
    -- for (local i = start, j = endp - dim; i < endp; i += dim) {
   	local j = endp - dim + 1
   	for i = start + 1, endp, dim do
        sum = sum + (data[j] - data[i]) * (data[i + 1] + data[j + 1])
        j = i
    end
    return sum
end

-- turn a polygon in a multi-dimensional array form (e.g. as in GeoJSON) into a form Earcut accepts
function Earcut.flatten(data)
    local dim = #data[1][1]
    local result = {vertices = {}, holes = {}, dimensions = dim}
    local holeIndex = 0

    -- for (local i = 0 i < data.length i++) {
   	for i = 1, #data do
        -- for (local j = 0 j < data[i].length j++) {
        for j = 1, #data[i] do
            -- for (local d = 0 d < dim d++) result.vertices.push(data[i][j][d])
            for d = 1, dim do
            	_push(result.vertices, data[i][j][d])
            end
        end
        if (i > 1) then
            holeIndex = holeIndex + #data[i - 1]
            _.push(result.holes, holeIndex)
        end
    end
    return result
end

return Earcut
--]]
