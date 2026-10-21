-- Vanilla Lua 5.0 compatible. No chat scraping, executable payloads or DB access.
pfQuestPartyBots = { roster = {}, snapshots = {}, pending = {}, listeners = {}, wanted = {}, errors = {} }
local P = pfQuestPartyBots
P.prefix, P.timeout, P.interval, P.staleAfter = "PFQBOT", 15, 15, 45
local token = math.floor(GetTime() * 1000) + math.mod(time(), 1000000) * 1000
local lastSent, nextSend, nextPoll, lastRefresh = {}, 0, 0, 0
local initialized = false

function P.Split(text, delimiter)
  local result, start = {}, 1
  while true do
    local first, last = string.find(text, delimiter, start, true)
    if not first then table.insert(result, string.sub(text, start)); return result end
    table.insert(result, string.sub(text, start, first - 1))
    start = last + 1
  end
end

local function Integer(text, signed)
  if type(text) ~= "string" or not string.find(text, signed and "^%-?%d+$" or "^%d+$") then return nil end
  local value = tonumber(text)
  if not value or value > 4294967295 or value < (signed and -2147483648 or 0) then return nil end
  return value
end

local function Unhex(text)
  if math.mod(string.len(text), 2) ~= 0 or string.len(text) > 8192 or
    string.find(text, "[^0-9a-f]") then return nil end
  return string.gsub(text, "..", function(pair) return string.char(tonumber(pair, 16)) end)
end

function P.ParseSnapshot(body)
  if string.len(body) > 24576 then return nil end
  local records, count = {}, 0
  if body == "" then return records end
  if string.sub(body, -1) ~= "\n" then return nil end
  local lines = P.Split(string.sub(body, 1, -2), "\n")
  for _, line in ipairs(lines) do
    local f = P.Split(line, "|")
    local id, level = Integer(f[1]), Integer(f[3])
    if table.getn(f) ~= 5 or not id or id == 0 or not level or level > 255 or records[id] or
      not string.find(f[2], "^[ICFRN]$") then return nil end
    local title = Unhex(f[4])
    if not title or title == "" or string.find(title, "[%z\1-\31]") then return nil end
    local record = { id = id, state = f[2], level = level, title = title, objectives = {} }
    if f[5] ~= "" then
      local seen = {}
      if record.state == "R" or record.state == "N" then return nil end
      for _, raw in ipairs(P.Split(f[5], ";")) do
        local o = P.Split(raw, ",")
        local index, entry, current, required = Integer(o[2]), Integer(o[3], true), Integer(o[4]), Integer(o[5])
        if table.getn(o) ~= 5 or not string.find(o[1], "^[IKGEM]$") or not index or index < 1 or index > 4 or
          not entry or not current or not required or required < 1 or table.getn(record.objectives) >= 10 or
          seen[o[1] .. index] then return nil end
        seen[o[1] .. index] = true
        table.insert(record.objectives, { kind = o[1], index = index, entry = entry, current = current, required = required })
      end
    end
    records[id], count = record, count + 1
    if count > 50 then return nil end
  end
  return records
end

function P.Notify()
  for _, callback in ipairs(P.listeners) do callback() end
end

function P.BuildRoster()
  local roster = {}
  local raid = GetNumRaidMembers() > 0
  local count = raid and GetNumRaidMembers() or GetNumPartyMembers()
  for i = 1, count do
    local unit = (raid and "raid" or "party") .. i
    local name = UnitName(unit)
    if name and name ~= "Unknown" and not UnitIsUnit(unit, "player") then
      table.insert(roster, { name = name, unit = unit, online = not UnitIsConnected or UnitIsConnected(unit) })
    end
  end
  local signature, old = "", ""
  for _, member in ipairs(roster) do signature = signature .. member.name .. ":" .. tostring(member.online) .. ";" end
  for _, member in ipairs(P.roster) do old = old .. member.name .. ":" .. tostring(member.online) .. ";" end
  if signature ~= old then
    P.roster, P.pending, P.wanted, lastSent, nextPoll = roster, {}, {}, {}, 0
    P.Notify()
  end
end

function P.Member(name)
  for _, member in ipairs(P.roster) do if member.name == name then return member end end
end

function P.QuestId(key, quest)
  local number = tonumber(key)
  if not number and quest and quest.qlogid and GetQuestLink then
    local link = GetQuestLink(quest.qlogid)
    if link then local _, _, value = string.find(link, "quest:(%d+)"); number = tonumber(value) end
  end
  return number
end

function P.PlayerQuestIds()
  local ids, seen = {}, {}
  for id, quest in pairs((pfQuest and pfQuest.questlog) or {}) do
    local number = P.QuestId(id, quest)
    if number and number > 0 and not seen[number] then table.insert(ids, number); seen[number] = true end
  end
  table.sort(ids)
  return ids
end

function P.Request(member, now)
  token = math.mod(token, 4294967294) + 1
  local snapshot = P.snapshots[member.name]
  local hash = snapshot and snapshot.hash or "0"
  local header = "Q|1|" .. token .. "|" .. member.name .. "|" .. hash .. "|"
  local query = ""
  local count = 0
  for _, id in ipairs(P.PlayerQuestIds()) do
    local value = query .. (query == "" and "" or ",") .. id
    if string.len(header .. value) <= 240 and count < 25 then query, count = value, count + 1 end
  end
  P.pending[member.name] = { token = tostring(token), deadline = now + P.timeout, chunks = {}, count = 0 }
  lastSent[member.name] = now
  P.wanted[member.name] = nil
  SendAddonMessage(P.prefix, header .. query, GetNumRaidMembers() > 0 and "RAID" or "PARTY")
end

local function Complete(name, pending, hash, records, body)
  local now = GetTime()
  local previous = P.snapshots[name]
  body = body or (previous and previous.body)
  local snapshot = { hash = hash, records = records, body = body, seen = now, received = time(), error = nil }
  P.snapshots[name], P.pending[name] = snapshot, nil
  P.errors[name] = nil
  pfQuestPartyBotsDB.snapshots[name] = { hash = hash, body = body, received = snapshot.received }
  -- Keep at most one raid's snapshots across party changes and reloads.
  local count, oldest, oldestTime = 0, nil, time() + 1
  for other, saved in pairs(pfQuestPartyBotsDB.snapshots) do
    count = count + 1
    if not P.Member(other) and (tonumber(saved.received) or 0) < oldestTime then
      oldest, oldestTime = other, tonumber(saved.received) or 0
    end
  end
  if count > 40 and oldest then P.snapshots[oldest], pfQuestPartyBotsDB.snapshots[oldest] = nil, nil end
  P.Notify()
end

function P.Receive(prefix, message, channel, sender)
  -- Native Player::SendAddonMessage delivers GUILD locally with our own identity.
  if prefix ~= P.prefix or channel ~= "GUILD" or sender ~= UnitName("player") or
    type(message) ~= "string" or string.len(message) > 240 then return false end
  local _, _, kind, requestToken, name = string.find(message, "^([SUE])|1|(%d+)|([^|]+)|")
  local pending = name and P.pending[name]
  local member = name and P.Member(name)
  if not pending or pending.token ~= requestToken or not member or not member.online or GetTime() > pending.deadline then return false end
  local function Invalid()
    P.pending[name] = nil
    P.errors[name] = "Invalid or incomplete server reply"
    if P.snapshots[name] then P.snapshots[name].error = "Invalid reply" end
    P.Notify()
    return false
  end
  if kind == "E" then
    local result = Invalid()
    P.errors[name] = "Server snapshot exceeded its size limit"
    return result
  end
  if kind == "U" then
    local _, _, hash = string.find(message, "^U|1|%d+|[^|]+|([0-9a-f]+)$")
    local snapshot = P.snapshots[name]
    if not hash or string.len(hash) ~= 16 or not snapshot or snapshot.hash ~= hash or pending.count > 0 then return Invalid() end
    Complete(name, pending, hash, snapshot.records)
    return true
  end
  local _, _, hash, part, total, payload = string.find(message, "^S|1|%d+|[^|]+|([0-9a-f]+)|(%d+)|(%d+)|(.*)$")
  local index, parts = Integer(part), Integer(total)
  if not hash or string.len(hash) ~= 16 or not index or not parts or parts < 1 or parts > 192 or
    index < 1 or index > parts or string.len(payload) > 128 or
    (pending.hash and (hash ~= pending.hash or parts ~= pending.total)) then return Invalid() end
  if pending.chunks[index] and pending.chunks[index] ~= payload then return Invalid() end
  pending.hash, pending.total = hash, parts
  if not pending.chunks[index] then pending.chunks[index], pending.count = payload, pending.count + 1 end
  if pending.count ~= parts then return true end
  local body = table.concat(pending.chunks)
  local records = P.ParseSnapshot(body)
  if not records then return Invalid() end
  Complete(name, pending, hash, records, body)
  return true
end

P.labels = { I = "In progress", C = "Ready", F = "Failed", R = "Turned in", N = "Not in log" }
function P.Status(name, id)
  local member, snapshot = P.Member(name), P.snapshots[name]
  if not member or not member.online then return "Offline", nil, true end
  if not snapshot then return P.pending[name] and "Pending" or "Unavailable", nil, true end
  local record = snapshot.records[id]
  local label = P.labels[record and record.state or "N"]
  if record and record.state == "I" and table.getn(record.objectives) > 0 then
    local current, required = 0, 0
    for _, o in ipairs(record.objectives) do current = current + math.min(o.current, o.required); required = required + o.required end
    label = math.floor(current / required * 100) .. "%"
  end
  local stale = not snapshot.seen or snapshot.error or GetTime() - snapshot.seen > P.staleAfter
  return label, record, stale
end

function P.Refresh()
  if GetTime() - lastRefresh < 2 then return end
  lastRefresh, nextPoll = GetTime(), 0
  for _, member in ipairs(P.roster) do P.wanted[member.name] = true end
end

local frame = CreateFrame("Frame", "pfQuestPartyBotsEvents", UIParent)
frame:RegisterEvent("PLAYER_LOGIN")
frame:RegisterEvent("PLAYER_ENTERING_WORLD")
frame:RegisterEvent("PARTY_MEMBERS_CHANGED")
frame:RegisterEvent("RAID_ROSTER_UPDATE")
frame:RegisterEvent("CHAT_MSG_ADDON")
frame:RegisterEvent("QUEST_LOG_UPDATE")
frame:RegisterEvent("PARTY_MEMBER_ENABLE")
frame:RegisterEvent("PARTY_MEMBER_DISABLE")
frame:SetScript("OnEvent", function()
  if not initialized then
    if type(pfQuestPartyBotsDB) ~= "table" or pfQuestPartyBotsDB.version ~= 1 then pfQuestPartyBotsDB = { version = 1, snapshots = {} } end
    if type(pfQuestPartyBotsDB.snapshots) ~= "table" then pfQuestPartyBotsDB.snapshots = {} end
    for name, snapshot in pairs(pfQuestPartyBotsDB.snapshots) do
      if type(snapshot) == "table" and type(snapshot.body) == "string" and type(snapshot.hash) == "string" and
        string.len(snapshot.hash) == 16 and not string.find(snapshot.hash, "[^0-9a-f]") then
        local records = P.ParseSnapshot(snapshot.body)
        if records then P.snapshots[name] = { records = records, body = snapshot.body, hash = snapshot.hash, received = snapshot.received } end
      end
    end
    initialized = true
  end
  if event == "CHAT_MSG_ADDON" then P.Receive(arg1, arg2, arg3, arg4)
  elseif event == "QUEST_LOG_UPDATE" then P.Refresh()
  else P.BuildRoster() end
end)
frame:SetScript("OnUpdate", function()
  if not initialized then return end
  local now = GetTime()
  if now < nextSend then return end
  nextSend = now + 0.5
  for name, pending in pairs(P.pending) do
    if now > pending.deadline then
      P.pending[name] = nil
      P.errors[name] = "No current server reply"
      if P.snapshots[name] then P.snapshots[name].error = "No current reply" end
      P.Notify()
    end
  end
  if now < nextPoll then return end
  for _, member in ipairs(P.roster) do
    if member.online and not P.pending[member.name] and (not lastSent[member.name] or
      now - lastSent[member.name] >= (P.wanted[member.name] and 6 or P.interval)) then
      P.Request(member, now)
      return
    end
  end
  nextPoll = now + 1
end)
