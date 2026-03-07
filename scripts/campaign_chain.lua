function on_mission_start()
  set_campaign_flag("intro_seen", true)
  add_campaign_resource("Food", 25)
  set_campaign_branch("route_alpha")
  unlock_campaign_reward("opening_badge")
  show_message("Campaign opening hook executed")
end

function on_alpha_start()
  local prev = get_previous_mission_result()
  local introSeen = get_campaign_flag("intro_seen")
  if prev == "victory" and introSeen then
    add_campaign_resource("Wealth", 10)
    show_message("Alpha branch validated from carryover")
  end
end
