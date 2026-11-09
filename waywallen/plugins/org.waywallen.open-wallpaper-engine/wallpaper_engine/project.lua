local M = {}

M.WE_APPID = "431960"
M.WORKSHOP = "/steamapps/workshop/content/" .. M.WE_APPID
M.ASSETS_REL = "/steamapps/common/wallpaper_engine/assets"
M.PROJECTS_REL = "/steamapps/common/wallpaper_engine/projects"
M.LOCAL_DIRS = { "defaultprojects", "myprojects" }

-- Users are asked to register the directory that *contains* steamapps, but they
-- routinely pick steamapps itself, or the workshop content directory they can
-- see the wallpapers in. Fold those back to the Steam root, so the paths built
-- from WORKSHOP/PROJECTS_REL still resolve instead of silently missing.
function M.normalize_root(path)
    if type(path) ~= "string" then return path end
    local root = path:gsub("/+$", "")
    root = root:gsub("/steamapps/workshop/content/" .. M.WE_APPID .. "$", "")
    root = root:gsub("/steamapps/workshop/content$", "")
    root = root:gsub("/steamapps/workshop$", "")
    root = root:gsub("/steamapps$", "")
    return root
end

local VIDEO_EXTS = { mp4 = true, webm = true, mkv = true, avi = true, mov = true }

local SCENE_BASE_BY_FILE = {
    ["assets.json"] = "assets",
    ["audiophile.json"] = "audiophile",
    ["fantasticcar.json"] = "fantasticcar",
    ["gifscene.json"] = "gifscene",
    ["ricepod.json"] = "ricepod",
    ["scene.json"] = "scene",
    ["techno.json"] = "techno",
}

function M.pick_preview(ctx, dir, project)
    if project and project.preview then
        local p = dir .. "/" .. project.preview
        if ctx.fs.exists(p) then return p end
    end
    for _, p in ipairs({ dir .. "/preview.jpg", dir .. "/preview.png", dir .. "/preview.gif" }) do
        if ctx.fs.exists(p) then return p end
    end
    return nil
end

function M.classify(ctx, dir, project, project_type)
    if project_type == "web" then
        if ctx.fs.exists(dir .. "/project.json") then
            return "web", dir
        end
    elseif project_type == "video" then
        local file = project and project.file
        if file and ctx.fs.exists(dir .. "/" .. file) then
            return "video", dir .. "/" .. file
        end
        for _, path in ipairs(ctx.fs.glob(dir .. "/*.*")) do
            local ext = ctx.fs.extension(path)
            if ext and VIDEO_EXTS[string.lower(ext)] then
                return "video", path
            end
        end
    else
        local file = project and project.file
        local base = SCENE_BASE_BY_FILE[file] or "scene"
        if ctx.fs.exists(dir .. "/" .. base .. ".pkg") then
            return "scene", dir .. "/" .. base .. ".pkg"
        elseif ctx.fs.exists(dir .. "/" .. base .. ".json") then
            return "scene", dir .. "/" .. base .. ".json"
        end
    end
    return nil, nil
end

local function read_project(ctx, dir)
    local path = dir .. "/project.json"
    if not ctx.fs.exists(path) then return nil end
    local content = ctx.fs.read(path)
    if not content then return nil end
    local project = ctx.json.parse(content)
    if type(project) ~= "table" then return nil end
    return project
end

-- A Workshop preset is a project.json with no type that names the item it is
-- made from ("dependency", a sibling directory) and the property values it sets
-- ("preset"); Wallpaper Engine plays that item with those values. Returns the
-- parent's directory and project.
function M.preset_parent(ctx, dir, project)
    if type(project) ~= "table" or project.type ~= nil or project.dependency == nil then
        return nil
    end
    local container = dir:match("^(.*)/[^/]+/*$")
    if not container then return nil end
    local parent_dir = container .. "/" .. tostring(project.dependency)
    local parent = read_project(ctx, parent_dir)
    if not parent then return nil end
    return parent_dir, parent
end

local function project_type(project)
    return project and type(project.type) == "string" and string.lower(project.type) or nil
end

-- What a preset is listed as: its parent's type, and its own project.json as the
-- resource (the parent's resource is the parent's entry).
function M.classify_preset(ctx, dir, project)
    local parent_dir, parent = M.preset_parent(ctx, dir, project)
    if not parent_dir then return nil, nil end
    local wp_type = M.classify(ctx, parent_dir, parent, project_type(parent))
    if not wp_type then return nil, nil end
    return wp_type, dir .. "/project.json"
end

-- What an entry plays: its directory and resource, and for a preset its parent's
-- with the preset's values.
function M.resolve(ctx, entry)
    local dir = entry.resource and entry.resource:match("^(.*)/project%.json$")
    if dir then
        local project = read_project(ctx, dir)
        local parent_dir, parent = M.preset_parent(ctx, dir, project)
        if parent_dir then
            local _, resource = M.classify(ctx, parent_dir, parent, project_type(parent))
            local preset = type(project.preset) == "table" and project.preset or {}
            return { dir = parent_dir, resource = resource, preset = preset }
        end
    end
    return { dir = M.project_dir_of(entry), resource = entry.resource }
end

-- Strip the item-directory prefix so resource/preview come back relative to it,
-- the form the daemon's resolve upsert expects.
function M.relpath(dir, abs)
    if not abs then
        return nil
    end
    local d = dir:gsub("/+$", "")
    if abs == d then
        return ""
    end
    local prefix = d .. "/"
    if abs:sub(1, #prefix) == prefix then
        return abs:sub(#prefix + 1)
    end
    return abs
end

function M.project_dir_of(entry)
    if entry.wp_type == "web" then return entry.resource end
    return entry.resource and entry.resource:match("^(.*)/[^/]+$") or nil
end

return M
