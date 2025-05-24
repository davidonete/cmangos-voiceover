setfenv(1, VoiceOver)

---@class Addon : AceAddon, AceAddon-3.0, AceEvent-3.0, AceTimer-3.0
---@field db VoiceOverConfig|AceDBObject-3.0
Addon = LibStub("AceAddon-3.0"):NewAddon("VoiceOver", "AceEvent-3.0", "AceTimer-3.0")

Addon.OnAddonLoad = {}

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
    self:RegisterEvent("CHAT_MSG_ADDON")
    --[[
    self:RegisterEvent("QUEST_DETAIL")
    self:RegisterEvent("QUEST_PROGRESS")
    self:RegisterEvent("QUEST_COMPLETE")
    self:RegisterEvent("QUEST_GREETING")
    self:RegisterEvent("QUEST_FINISHED")
    self:RegisterEvent("GOSSIP_SHOW")
    self:RegisterEvent("GOSSIP_CLOSED")
    ]]

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
	if Addon.initialized == false then
		Addon:SendServerMessage("enableAddon")
        Addon.initialized = true
	end
end

function Addon:HandleQuestGossip(questID, status)
    DEFAULT_CHAT_FRAME:AddMessage("HandleQuestGossip questID: " .. questID .. " status: " .. status)
	local questTitle = GetTitleText()
	local questText = GetQuestText()
	local guid = Utils:GetNPCGUID()
	local targetName = Utils:GetNPCName()

	local questEvent = Enums.SoundEvent.QuestGreeting
	if status == "1" then
		questEvent = Enums.SoundEvent.QuestAccept
	elseif status == "3" then
		questEvent = Enums.SoundEvent.QuestComplete
	end
			
	local soundData = 
	{
		event = questEvent,
		questID = questID,
		name = targetName,
		title = questTitle,
		text = questText,
		unitGUID = guid,
		unitIsObjectOrItem = Utils:IsNPCObjectOrItem(),
		addedCallback = QuestSoundDataAdded,
	}

    for key, value in pairs(soundData) do
        DEFAULT_CHAT_FRAME:AddMessage(tostring(key) .." = " .. tostring(value))
    end

	SoundQueue:AddSoundToQueue(soundData)
end

function Addon:HandleServerMessage(msg)
    --DEFAULT_CHAT_FRAME:AddMessage("HandleServerMessage " .. msg)
	local args = self:Explode(msg, "#")
	if string.find(msg, "AddonEnabled", 1, true) then
		--self:HandleInitialize()
	elseif string.find(msg, "Quest", 1, true) then
		-- Quest#questID;status
		args = self:Explode(args[2], ";")
		self:HandleQuestGossip(args[1], args[2])
	end
end

function Addon:CHAT_MSG_ADDON()
	if string.find(arg1, self.serverMessagePrefix, 1, true) then
        self:HandleServerMessage(arg2)
	end
end

function Addon:ADDON_LOADED(event, addon)
    addon = addon or arg1 -- Thanks, Ace3v...
    local hook = self.OnAddonLoad[addon]
    if hook then
        hook()
    end

    self.SendInitializeRequest()
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

--[[
local GetTitleText = GetTitleText -- Store original function before EQL3 (Extended Quest Log 3) overrides it and starts prepending quest level
function Addon:QUEST_DETAIL()
    local questID = GetQuestID()
    local questTitle = GetTitleText()
    local questText = GetQuestText()
    local guid = Utils:GetNPCGUID()
    local targetName = Utils:GetNPCName()

    if not questID or questID == 0 then
        return
    end

    -- Can happen if the player interacted with an NPC while having main menu or options opened
    if not guid and not targetName then
        return
    end

    if Addon.db.char.RecentQuestTitleToID and questID ~= 0 then
        Addon.db.char.RecentQuestTitleToID[questTitle] = questID
    end

    local type = guid and Utils:GetGUIDType(guid)
    if type == Enums.GUID.Item then
        -- Allow quests started from items to have VO, book icon will be displayed for them
    elseif not type or not Enums.GUID:CanHaveID(type) then
        -- If the quest is started by something that we cannot extract the ID of (e.g. Player, when sharing a quest) - try to fallback to a questgiver from a module's database
        local id
        type, id = DataModules:GetQuestLogQuestGiverTypeAndID(questID)
        guid = id and Enums.GUID:CanHaveID(type) and Utils:MakeGUID(type, id) or guid
        targetName = id and DataModules:GetObjectName(type, id) or targetName or "Unknown Name"
    end

    -- print("QUEST_DETAIL", questID, questTitle);
    ---@type SoundData
    local soundData = {
        event = Enums.SoundEvent.QuestAccept,
        questID = questID,
        name = targetName,
        title = questTitle,
        text = questText,
        unitGUID = guid,
        unitIsObjectOrItem = Utils:IsNPCObjectOrItem(),
        addedCallback = QuestSoundDataAdded,
    }

	--for k, v in pairs(soundData) do
	--	DEFAULT_CHAT_FRAME:AddMessage(tostring(k) .. " = " .. tostring(v))
	--end
	
    --SoundQueue:AddSoundToQueue(soundData)
end

function Addon:QUEST_COMPLETE()
    local questID = GetQuestID()
    local questTitle = GetTitleText()
    local questText = GetRewardText()
    local guid = Utils:GetNPCGUID()
    local targetName = Utils:GetNPCName()

    if not questID or questID == 0 then
        return
    end

    -- Can happen if the player interacted with an NPC while having main menu or options opened
    if not guid and not targetName then
        return
    end

    if Addon.db.char.RecentQuestTitleToID and questID ~= 0 then
        Addon.db.char.RecentQuestTitleToID[questTitle] = questID
    end

    -- print("QUEST_COMPLETE", questID, questTitle);
    ---@type SoundData
    local soundData = {
        event = Enums.SoundEvent.QuestComplete,
        questID = questID,
        name = targetName,
        title = questTitle,
        text = questText,
        unitGUID = guid,
        unitIsObjectOrItem = Utils:IsNPCObjectOrItem(),
        addedCallback = QuestSoundDataAdded,
    }
    --SoundQueue:AddSoundToQueue(soundData)
end
]]

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