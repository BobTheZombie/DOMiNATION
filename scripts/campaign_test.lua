function on_mission_start()
  show_message("[briefing] Lua hook: mission orchestration online")
  if get_campaign_flag("intro_seen") == false then
    set_campaign_flag("intro_seen", true)
    show_message("[intelligence] First contact marker acknowledged")
  end
end
