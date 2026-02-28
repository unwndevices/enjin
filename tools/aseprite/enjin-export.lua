-- enjin-export.lua — Aseprite plugin: export indexed-color sprites to enjin C header format.
--
-- Usage: Place in Aseprite's scripts directory (Edit > Preferences > Scripting),
--        then run via File > Scripts > enjin-export.
--
-- Output format matches tools/aseprite2enjin.py exactly:
--   const uint8_t name_data[] with 16 hex values per line, lower nibble masked,
--   transparent index remapped to 15.

-- ---------------------------------------------------------------------------
-- Validation
-- ---------------------------------------------------------------------------

local function validate_sprite(sprite)
    if not sprite then
        app.alert("No sprite is open. Please open an indexed-color .aseprite file first.")
        return false
    end
    if sprite.colorMode ~= ColorMode.INDEXED then
        app.alert(
            "This sprite is not in indexed color mode.\n" ..
            "To convert: Sprite > Color Mode > Indexed."
        )
        return false
    end
    return true
end

-- ---------------------------------------------------------------------------
-- Pixel extraction — composites all visible layers for one frame
-- ---------------------------------------------------------------------------

local function extract_frame_pixels(sprite, frame_index)
    local w = sprite.width
    local h = sprite.height
    local transparent_idx = sprite.transparentColor

    -- Pre-fill canvas with transparent index (will be remapped to 15 later)
    local buffer = {}
    for i = 1, w * h do
        buffer[i] = transparent_idx
    end

    -- Composite all visible layers onto the buffer
    for _, layer in ipairs(sprite.layers) do
        if layer.isVisible then
            local cel = layer:cel(frame_index)
            if cel then
                local image = cel.image
                local pos   = cel.position
                for cy = 0, image.height - 1 do
                    for cx = 0, image.width - 1 do
                        local px = image:getPixel(cx, cy)
                        local canvas_x = pos.x + cx
                        local canvas_y = pos.y + cy
                        if canvas_x >= 0 and canvas_x < w and canvas_y >= 0 and canvas_y < h then
                            buffer[canvas_y * w + canvas_x + 1] = px
                        end
                    end
                end
            end
        end
    end

    -- Remap transparent color to index 15 (swap to avoid data loss)
    if transparent_idx ~= 15 then
        for i = 1, w * h do
            local v = buffer[i]
            if v == transparent_idx then
                buffer[i] = 15
            elseif v == 15 then
                buffer[i] = transparent_idx
            end
        end
    end

    -- Apply lower nibble mask to every pixel
    for i = 1, w * h do
        buffer[i] = buffer[i] & 0x0F
    end

    return buffer
end

-- ---------------------------------------------------------------------------
-- Build pixel data — handles grid, single-frame, and multi-frame modes
-- ---------------------------------------------------------------------------

local function build_pixel_data(sprite, grid_w, grid_h)
    local w = sprite.width
    local h = sprite.height

    if grid_w > 0 and grid_h > 0 then
        -- Grid mode: treat first frame as a spritesheet divided into cells
        local cols = math.floor(w / grid_w)
        local rows = math.floor(h / grid_h)
        if cols == 0 or rows == 0 then
            app.alert(string.format(
                "Warning: grid %dx%d does not fit within canvas %dx%d; using 1x1.",
                grid_w, grid_h, w, h
            ))
            cols = math.max(1, cols)
            rows = math.max(1, rows)
        end
        if w % grid_w ~= 0 or h % grid_h ~= 0 then
            app.alert(string.format(
                "Warning: grid %dx%d does not evenly divide canvas %dx%d; cells will be truncated.",
                grid_w, grid_h, w, h
            ))
        end

        local frame_pixels = extract_frame_pixels(sprite, 1)

        -- Extract cells in row-major order, matching Python's build_pixel_array grid logic
        local out = {}
        for row = 0, rows - 1 do
            for col = 0, cols - 1 do
                for py = 0, grid_h - 1 do
                    for px = 0, grid_w - 1 do
                        local src_x = col * grid_w + px
                        local src_y = row * grid_h + py
                        if src_x < w and src_y < h then
                            out[#out + 1] = frame_pixels[src_y * w + src_x + 1]
                        else
                            out[#out + 1] = 0x0F  -- transparent
                        end
                    end
                end
            end
        end

        return out, grid_w, grid_h, cols, rows

    elseif #sprite.frames == 1 then
        -- Single frame: emit frame pixels as-is (mask already applied in extract_frame_pixels)
        local frame_pixels = extract_frame_pixels(sprite, 1)
        return frame_pixels, w, h, 1, 1

    else
        -- Multi-frame animation: concatenate all frame buffers
        local out = {}
        for frame_idx = 1, #sprite.frames do
            local frame_pixels = extract_frame_pixels(sprite, frame_idx)
            for _, v in ipairs(frame_pixels) do
                out[#out + 1] = v
            end
        end
        return out, w, h, #sprite.frames, 1
    end
end

-- ---------------------------------------------------------------------------
-- C header emitter
-- ---------------------------------------------------------------------------

local function emit_header(pixels, name, cell_w, cell_h, cols, rows, source_filename)
    local total_frames = cols * rows
    local frame_size   = cell_w * cell_h
    local parts        = {}

    -- Header comments
    parts[#parts + 1] = "// Generated by enjin-export (Aseprite plugin) from " .. source_filename
    parts[#parts + 1] = string.format("// Cell: %dx%d, Grid: %dx%d, Frames: %d",
        cell_w, cell_h, cols, rows, total_frames)
    parts[#parts + 1] = "#pragma once"
    parts[#parts + 1] = "#include <cstdint>"
    parts[#parts + 1] = ""
    parts[#parts + 1] = string.format("const uint8_t %s_data[] = {", name)

    -- Frame data
    for frame_idx = 0, total_frames - 1 do
        parts[#parts + 1] = string.format("    // Frame %d", frame_idx)

        local start = frame_idx * frame_size  -- 0-based offset into pixels table
        for i = 0, frame_size - 1, 16 do
            -- Build up to 16 hex values for this output line
            local vals = {}
            local count = math.min(16, frame_size - i)
            for j = 0, count - 1 do
                vals[#vals + 1] = string.format("0x%02X", pixels[start + i + j + 1])
            end

            -- Trailing comma on every line except the very last data line of the last frame
            local is_last_frame = (frame_idx + 1 == total_frames)
            local is_last_line  = (i + 16 >= frame_size)
            local comma = (not (is_last_frame and is_last_line)) and "," or ""

            parts[#parts + 1] = "    " .. table.concat(vals, ", ") .. comma
        end
    end

    parts[#parts + 1] = "};"
    parts[#parts + 1] = ""
    parts[#parts + 1] = "// Usage:"
    parts[#parts + 1] = "// #include \"enjin2/graphics/sprite.hpp\""
    parts[#parts + 1] = string.format(
        "// enjin2::SpriteSheet %s(%s_data, %d, %d, %d, %d);",
        name, name, cell_w, cell_h, cols, rows)
    parts[#parts + 1] = ""

    return table.concat(parts, "\n")
end

-- ---------------------------------------------------------------------------
-- Name derivation — C identifier from file path
-- ---------------------------------------------------------------------------

local function derive_name(filepath)
    local base = app.fs.fileTitle(filepath)
    -- Replace non-alphanumeric/underscore characters with underscore
    local ident = base:gsub("[^%w_]", "_")
    -- Prepend underscore if starts with a digit
    if ident:match("^%d") then
        ident = "_" .. ident
    end
    -- Fallback
    if ident == "" then
        ident = "sprite"
    end
    return ident
end

-- ---------------------------------------------------------------------------
-- Dialog and main export function
-- ---------------------------------------------------------------------------

local function run_export()
    local sprite = app.sprite

    -- Validate sprite
    if not validate_sprite(sprite) then
        return
    end

    -- Derive defaults
    local default_name = derive_name(sprite.filename)

    local file_path = app.fs.filePath(sprite.filename)
    local file_title = app.fs.fileTitle(sprite.filename)
    local default_output = file_path .. app.fs.pathSeparator .. file_title .. ".h"

    -- Build dialog
    local dlg = Dialog("enjin Export")

    dlg:entry{
        id    = "output",
        label = "Output .h path:",
        text  = default_output,
    }
    dlg:entry{
        id    = "name",
        label = "C identifier:",
        text  = default_name,
    }
    dlg:separator{ text = "Grid Mode (optional)" }
    dlg:number{
        id       = "grid_w",
        label    = "Cell width:",
        text     = "0",
        decimals = 0,
    }
    dlg:number{
        id       = "grid_h",
        label    = "Cell height:",
        text     = "0",
        decimals = 0,
    }
    dlg:button{ id = "export", text = "Export" }
    dlg:button{ id = "cancel", text = "Cancel" }

    dlg:show()

    -- Check if user pressed Export
    local data = dlg.data
    if not data.export then
        return
    end

    local output_path = data.output
    local name        = data.name
    local grid_w      = math.floor(data.grid_w or 0)
    local grid_h      = math.floor(data.grid_h or 0)

    -- Validate name
    if not name or name == "" then
        name = default_name
    end
    -- Ensure valid C identifier: replace non-alphanumeric/underscore
    name = name:gsub("[^%w_]", "_")
    if name:match("^%d") then
        name = "_" .. name
    end
    if name == "" then
        name = "sprite"
    end

    -- Validate output path
    if not output_path or output_path == "" then
        app.alert("Output path is empty. Export cancelled.")
        return
    end

    -- Build pixel data
    local pixels, cell_w, cell_h, cols, rows = build_pixel_data(sprite, grid_w, grid_h)

    -- Warn about transparent color remapping if needed
    if sprite.transparentColor ~= 15 then
        app.alert(string.format(
            "Note: sprite transparent color index is %d (not 15).\n" ..
            "Pixels with index %d were remapped to 15 in the output.\n" ..
            "Pixels with index 15 were remapped to %d.",
            sprite.transparentColor,
            sprite.transparentColor,
            sprite.transparentColor
        ))
    end

    -- Emit C header
    local source_filename = app.fs.fileName(sprite.filename)
    local header = emit_header(pixels, name, cell_w, cell_h, cols, rows, source_filename)

    -- Write file
    local f, err = io.open(output_path, "w")
    if not f then
        app.alert("Failed to write output file:\n" .. output_path .. "\n" .. (err or "unknown error"))
        return
    end
    f:write(header)
    f:close()

    -- Write .njn binary file
    local njn_path = output_path:gsub("%.h$", ".njn")
    if njn_path == output_path then
        njn_path = output_path .. ".njn"
    end
    
    local f2, err2 = io.open(njn_path, "wb")
    if f2 then
        local njn_header = string.char(78, 74, 1, 
            math.min(cell_w, 255), math.min(cell_h, 255), 
            math.min(cols, 255), math.min(rows, 255), 0)
        f2:write(njn_header)
        
        -- Write pixels in chunks or concat to avoid unpack limits
        local px_chars = {}
        for i, p in ipairs(pixels) do
            px_chars[i] = string.char(p)
        end
        f2:write(table.concat(px_chars))
        f2:close()
    else
        app.alert("Failed to write .njn file:\n" .. njn_path .. "\n" .. (err2 or "unknown error"))
    end

    -- Success summary
    local total_bytes = #pixels
    local total_frames = cols * rows
    app.alert(string.format(
    app.alert(string.format(
        "Export complete!\n\n" ..
        "Files:  %s\n" ..
        "        %s\n" ..
        "Array:  %s_data  (%d bytes)\n" ..
        "Cell:   %dx%d\n" ..
        "Grid:   %dx%d  (%d frames)",
        output_path, njn_path,
        name, total_bytes,
        cell_w, cell_h,
        cols, rows, total_frames
    ))
end

-- ---------------------------------------------------------------------------
-- Script entry point
-- ---------------------------------------------------------------------------

run_export()
