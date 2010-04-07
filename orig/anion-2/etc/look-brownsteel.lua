-- look-brownsteel.lua drawing engine configuration file for Ion.

if not gr_select_engine("de") then return end

de_reset()

de_define_style("*", {
    shadow_colour = "#404040",
    highlight_colour = "#707070",
    background_colour = "#505050",
    foreground_colour = "#a0a0a0",
    padding_pixels = 1,
    highlight_pixels = 1,
    shadow_pixels = 1,
    border_style = "elevated",
    font = "-*-helvetica-medium-r-normal-*-14-*-*-*-*-*-*-*",
    text_align = "center",
})

de_define_style("frame", {
    based_on = "*",
    shadow_colour = "#404040",
    highlight_colour = "#707070",
    padding_colour = "#505050",
    background_colour = "#000000",
    foreground_colour = "#ffffff",
    padding_pixels = 2,
    highlight_pixels = 1,
    shadow_pixels = 1,
    de_substyle("active", {
        shadow_colour = "#203040",
        highlight_colour = "#607080",
        padding_colour = "#405060",
        foreground_colour = "#ffffff",
    }),
})

de_define_style("frame-ionframe", {
    based_on = "frame",
    border_style = "inlaid",
    padding_pixels = 1,
    spacing = 1,
})

de_define_style("frame-floatframe", {
    based_on = "frame",
    border_style = "ridge"
})

de_define_style("tab", {
    based_on = "*",
    font = "-*-helvetica-medium-r-normal-*-12-*-*-*-*-*-*-*",
    de_substyle("active-selected", {
        shadow_colour = "#304050",
        highlight_colour = "#708090",
        background_colour = "#506070",
        foreground_colour = "#ffffff",
    }),
    de_substyle("active-unselected", {
        shadow_colour = "#203040",
        highlight_colour = "#607080",
        background_colour = "#405060",
        foreground_colour = "#a0a0a0",
    }),
    de_substyle("inactive-selected", {
        shadow_colour = "#404040",
        highlight_colour = "#909090",
        background_colour = "#606060",
        foreground_colour = "#a0a0a0",
    }),
    de_substyle("inactive-unselected", {
        shadow_colour = "#404040",
        highlight_colour = "#707070",
        background_colour = "#505050",
        foreground_colour = "#a0a0a0",
    }),
    text_align = "center",
})

de_define_style("tab-frame", {
    based_on = "tab",
    de_substyle("*-*-*-*-activity", {
        shadow_colour = "#404040",
        highlight_colour = "#707070",
        background_colour = "#990000",
        foreground_colour = "#eeeeee",
    }),
})

de_define_style("tab-frame-ionframe", {
    based_on = "tab-frame",
    spacing = 1,
})

de_define_style("tab-menuentry", {
    based_on = "tab",
    text_align = "left",
    highlight_pixels = 0,
    shadow_pixels = 0,
})

de_define_style("tab-menuentry-big", {
    based_on = "tab-menuentry",
    font = "-*-helvetica-medium-r-normal-*-17-*-*-*-*-*-*-*",
    padding_pixels = 7,
})

de_define_style("input", {
    based_on = "*",
    shadow_colour = "#404040",
    highlight_colour = "#707070",
    background_colour = "#000000",
    foreground_colour = "#ffffff",
    padding_pixels = 1,
    highlight_pixels = 1,
    shadow_pixels = 1,
    border_style = "elevated",
    de_substyle("*-cursor", {
        background_colour = "#ffffff",
        foreground_colour = "#000000",
    }),
    de_substyle("*-selection", {
        background_colour = "#505050",
        foreground_colour = "#ffffff",
    }),
})

de_define_style("input-menu", {
    based_on = "*",
    de_substyle("active", {
        shadow_colour = "#304050",
        highlight_colour = "#708090",
        background_colour = "#506070",
        foreground_colour = "#ffffff",
    }),
})

gr_refresh()

