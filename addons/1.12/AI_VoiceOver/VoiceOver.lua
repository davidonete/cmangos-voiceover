setfenv(1, VoiceOver)

---@class Addon : AceAddon, AceAddon-3.0, AceEvent-3.0, AceTimer-3.0
---@field db VoiceOverConfig|AceDBObject-3.0
Addon = LibStub("AceAddon-3.0"):NewAddon("VoiceOver", "AceEvent-3.0", "AceTimer-3.0")

Addon.OnAddonLoad = {}

Addon.QuestLog = {}

---@class VoiceOverConfig
local defaults = {
    profile = {
        SoundQueueUI = {
            LockFrame = false,
            FrameScale = 0.7,
            FrameStrata = "HIGH",
            HidePortrait = false,
            HideFrame = false,
        },
        Audio = {
            GossipFrequency = Enums.GossipFrequency.OncePerQuestNPC,
            SoundChannel = Enums.SoundChannel.Master,
            AutoToggleDialog = Version.IsLegacyVanilla or Version:IsRetailOrAboveLegacyVersion(60100),
            StopAudioOnDisengage = false,
        },
        MinimapButton = {
            LibDBIcon = {}, -- Table used by LibDBIcon to store position (minimapPos), dragging lock (lock) and hidden state (hide)
            Commands = {
                -- References keys from Options.table.args.SlashCommands.args table
                LeftButton = "Options",
                MiddleButton = "PlayPause",
                RightButton = "Clear",
            }
        },
        LegacyWrath = (Version.IsLegacyWrath or Version.IsLegacyBurningCrusade or nil) and {
            PlayOnMusicChannel = {
                Enabled = true,
                Volume = 1,
                FadeOutMusic = 0.5,
            },
            HDModels = false,
        },
        DebugEnabled = false,
    },
    char = {
        IsPaused = false,
        hasSeenGossipForNPC = {},
        RecentQuestTitleToID = Version:IsBelowLegacyVersion(30300) and {},
    }
}

Addon.serverMessagePrefix = "voiceover"
Addon.initialized = false

local lastGossipOptions
local selectedGossipOption
local currentQuestSoundData
local currentGossipSoundData

function Addon:SendServerMessage(msg)
	SendChatMessage("." .. self.serverMessagePrefix .. " " .. msg)
end

function Addon:OnInitialize()
    self.db = LibStub("AceDB-3.0"):New("VoiceOverDB", defaults)
    self.db.RegisterCallback(self, "OnProfileChanged", "RefreshConfig")
    self.db.RegisterCallback(self, "OnProfileReset", "RefreshConfig")

    StaticPopupDialogs["VOICEOVER_ERROR"] =
    {
        text = "VoiceOver|n|n%s",
        button1 = OKAY,
        timeout = 0,
        whileDead = 1,
    }

    SoundQueueUI:Initialize()
    DataModules:EnumerateAddons()
    Options:Initialize()

    self:RegisterEvent("ADDON_LOADED")
    self:RegisterEvent("PLAYER_ENTERING_WORLD")
    self:RegisterEvent("CHAT_MSG_ADDON")
	self:RegisterEvent("QUEST_DETAIL")
	self:RegisterEvent("QUEST_PROGRESS")
	self:RegisterEvent("QUEST_COMPLETE")

    if select(5, GetAddOnInfo("VoiceOver")) ~= "MISSING" then
        DisableAddOn("VoiceOver")
        if not self.db.profile.SeenDuplicateDialog then
            StaticPopupDialogs["VOICEOVER_DUPLICATE_ADDON"] =
            {
                text = [[VoiceOver|n|nTo fix the quest autoaccept bugs we had to rename the addon folder. If you're seeing this popup, it means the old one wasn't automatically removed.|n|nYou can safely delete "VoiceOver" from your Addons folder. "AI_VoiceOver" is the new folder.]],
                button1 = OKAY,
                timeout = 0,
                whileDead = 1,
                OnAccept = function()
                    self.db.profile.SeenDuplicateDialog = true
                end,
            }
            StaticPopup_Show("VOICEOVER_DUPLICATE_ADDON")
        end
    end

    if select(5, GetAddOnInfo("AI_VoiceOver_112")) ~= "MISSING" then
        DisableAddOn("AI_VoiceOver_112")
        if not self.db.profile.SeenDuplicateDialog112 then
            StaticPopupDialogs["VOICEOVER_DUPLICATE_ADDON_112"] =
            {
                text = [[VoiceOver|n|nVoiceOver port for 1.12 has been merged together with other versions and is no longer distributed as a separate addon.|n|nYou can safely delete "AI_VoiceOver_112" from your Addons folder. "AI_VoiceOver" is the new folder.]],
                button1 = OKAY,
                timeout = 0,
                whileDead = 1,
                OnAccept = function()
                    self.db.profile.SeenDuplicateDialog112 = true
                end,
            }
            StaticPopup_Show("VOICEOVER_DUPLICATE_ADDON_112")
        end
    end

    if not DataModules:HasRegisteredModules() then
        StaticPopupDialogs["VOICEOVER_NO_REGISTERED_DATA_MODULES"] =
        {
            text = [[VoiceOver|n|nNo sound packs were found.|n|nUse the "/vo options" command, (or Interface Options in newer clients) and go to the DataModules tab for information on where to download sound packs.]],
            button1 = OKAY,
            timeout = 0,
            whileDead = 1,
        }
        StaticPopup_Show("VOICEOVER_NO_REGISTERED_DATA_MODULES")
    end

    local function MakeAbandonQuestHook(field, getFieldData)
        return function()
            local data = getFieldData()
            local soundsToRemove = {}
            for _, soundData in pairs(SoundQueue.sounds) do
                if Enums.SoundEvent:IsQuestEvent(soundData.event) and soundData[field] == data then
                    table.insert(soundsToRemove, soundData)
                end
            end

            for _, soundData in pairs(soundsToRemove) do
                SoundQueue:RemoveSoundFromQueue(soundData)
            end
        end
    end

    if C_QuestLog and C_QuestLog.AbandonQuest then
        hooksecurefunc(C_QuestLog, "AbandonQuest", MakeAbandonQuestHook("questID", function() return C_QuestLog.GetAbandonQuest() end))
    elseif AbandonQuest then
        hooksecurefunc("AbandonQuest", MakeAbandonQuestHook("questName", function() return GetAbandonQuestName() end))
    end

    if QuestLog_Update then
        hooksecurefunc("QuestLog_Update", function()
            QuestOverlayUI:Update()
        end)
    end

    if C_GossipInfo and C_GossipInfo.SelectOption then
        hooksecurefunc(C_GossipInfo, "SelectOption", function(optionID)
            if lastGossipOptions then
                for _, info in ipairs(lastGossipOptions) do
                    if info.gossipOptionID == optionID then
                        selectedGossipOption = info.name
                        break
                    end
                end
                lastGossipOptions = nil
            end
        end)
    elseif SelectGossipOption then
        hooksecurefunc("SelectGossipOption", function(index)
            if lastGossipOptions then
                selectedGossipOption = lastGossipOptions[1 + (index - 1) * 2]
                lastGossipOptions = nil
            end
        end)
    end
end

function Addon:RefreshConfig()
    SoundQueueUI:RefreshConfig()
end

function Addon:Explode(str, delimiter)
    local result = {}
    local from = 1
    local delimFrom, delimTo = string.find(str, delimiter, from, 1, true)
    while delimFrom do
        table.insert(result, string.sub(str, from, delimFrom - 1))
        from = delimTo + 1
        delimFrom, delimTo = string.find(str, delimiter, from, true)
    end
    table.insert(result, string.sub(str, from))
    return result
end

function Addon:SendInitializeRequest()
    Addon:SendServerMessage("enableAddon")
end

function Addon:SendQuestLogRequest()
    Addon:SendServerMessage("questLog")
end

function Addon:SendSoundEventRequest(eventType, id, eventTitle)
    Addon:SendServerMessage("soundEvent "..eventType..";"..id..";"..eventTitle)
end

local function GossipSoundDataAdded(soundData)
    Utils:CreateNPCModelFrame(soundData)

    -- Save current gossip sound data for dialog/frame sync option
    currentGossipSoundData = soundData
end

local function QuestSoundDataAdded(soundData)
    Utils:CreateNPCModelFrame(soundData)

    -- Save current quest sound data for dialog/frame sync option
    currentQuestSoundData = soundData
end

function Addon:HandleInitialize()
    if Addon.initialized == false then
        Addon:SendQuestLogRequest()
        Addon.initialized = true
    end
end

-- Store original function before EQL3 (Extended Quest Log 3) overrides it and starts prepending quest level
local GetTitleText = GetTitleText 
function Addon:HandleSoundEvent(eventType, id, guid, eventTitle, targetName)
    if Enums.SoundEvent:IsQuestEvent(eventType) then
        if eventTitle == "" then
            eventTitle = GetTitleText()
        end

        if targetName == "" then
            targetName = Utils:GetNPCName()
        end

        local guidType = Utils:GetGUIDType(guid)
        local unitID = Utils:GetIDFromGUID(guid)

        local soundData = 
	    {
		    event = eventType,
		    questID = id,
		    name = targetName,
		    title = eventTitle,
            unitID = unitID,
		    unitGUID = guid,
		    unitIsObjectOrItem = guidType == Enums.GUID.Item or guidType == Enums.GUID.GameObject,
		    addedCallback = QuestSoundDataAdded
	    }
		
        SoundQueue:AddSoundToQueue(soundData)
    end
end

function Addon:HandleQuestLog(status, questID, guid, questTitle, questGiverName)
    if status == 1 then
        Addon.QuestLog[questTitle] =
        {
            id = questID,
            title = questTitle,
            questgiverGUID = guid,
            questGiverName = questGiverName
        }
    else
        Addon.QuestLog[questTitle] = nil
    end
end

function Addon:HandleServerMessage(msg)
	local args = self:Explode(msg, "#")
	if string.find(msg, "AddonEnabled", 1, true) then
		self:HandleInitialize()
	elseif string.find(msg, "SoundEvent", 1, true) then
		-- SoundEvent#Enums.SoundEvent;id;guid;eventTitle;targetName
		args = self:Explode(args[2], ";")
		self:HandleSoundEvent(tonumber(args[1]), tonumber(args[2]), args[3], args[4], args[5])
    elseif string.find(msg, "QuestLog", 1, true) then
        -- QuestLog#status;questID;guid;questTitle;questGiverName
		args = self:Explode(args[2], ";")
        self:HandleQuestLog(tonumber(args[1]), tonumber(args[2]), args[3], args[4], args[5])
	end
end

function Addon:QUEST_DETAIL()
	local questID = GetQuestID()
	local questTitle = GetTitleText()
	Addon:SendSoundEventRequest(Enums.SoundEvent.QuestAccept, questID, questTitle)
end

function Addon:QUEST_PROGRESS()
	local questID = 0
	local questTitle = GetTitleText()
	if questID == 0 then
		local questTitle = GetTitleText()
		local questInfo = Addon.QuestLog[questTitle];
		if questInfo ~= nil then
			questID = questInfo.id
		end
	end
	
	Addon:SendSoundEventRequest(Enums.SoundEvent.QuestProgress, questID, questTitle)
end

function Addon:QUEST_COMPLETE()
	local questID = GetQuestID()
	local questTitle = GetTitleText()
	if questID == 0 then
		local questTitle = GetTitleText()
		local questInfo = Addon.QuestLog[questTitle];
		if questInfo ~= nil then
			questID = questInfo.id
		end
	end
	
	Addon:SendSoundEventRequest(Enums.SoundEvent.QuestComplete, questID, questTitle)
end

function Addon:CHAT_MSG_ADDON()
	if string.find(arg1, self.serverMessagePrefix, 1, true) then
        Addon:HandleServerMessage(arg2)
	end
end

function Addon:PLAYER_ENTERING_WORLD(event, addon)
    Addon:SendInitializeRequest()
end

function Addon:ADDON_LOADED(event, addon)
    Addon:SendInitializeRequest()

    addon = addon or arg1 -- Thanks, Ace3v...
    local hook = self.OnAddonLoad[addon]
    if hook then
        hook()
    end
end

function Addon:ShouldPlayGossip(guid, text)
    local npcKey = guid or "unknown"

    local gossipSeenForNPC = self.db.char.hasSeenGossipForNPC[npcKey]

    if self.db.profile.Audio.GossipFrequency == Enums.GossipFrequency.OncePerQuestNPC then
        local numActiveQuests = GetNumGossipActiveQuests()
        local numAvailableQuests = GetNumGossipAvailableQuests()
        local npcHasQuests = (numActiveQuests > 0 or numAvailableQuests > 0)
        if npcHasQuests and gossipSeenForNPC then
            return
        end
    elseif self.db.profile.Audio.GossipFrequency == Enums.GossipFrequency.OncePerNPC then
        if gossipSeenForNPC then
            return
        end
    elseif self.db.profile.Audio.GossipFrequency == Enums.GossipFrequency.Never then
        return
    end

    return true, npcKey
end