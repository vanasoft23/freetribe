


# Ignored function branches to prevent spam
set $ignored_addresses = { 0xc0008f30, 0xc000985c, 0xc000ab54, 0xc0009a84 }





define addr_in_list
  # $arg0 = value to check
  # $arg1 = number of elements

  set $i = 0
  set $found = 0

  while $i < $arg1
    if $arg0 == $list[$i]
      set $found = 1
    end
    set $i = $i + 1
  end

  if $found
    printf "1\n"
  else
    printf "0\n"
  end
end




define stepi_silent
  set logging on
  stepi
  set logging off
end



#
# Logs all the branches of a codepath until given target address is hit, or
# until a maximum number of steps have been reached.
#
define trace_branch

  set verbose off
  set confirm off
  set print address off
  set print frame-arguments none
  set debug infrun 0

  set logging off
  set logging file NUL
  set logging redirect on

  #
  # Argument parsing
  #

  if $argc < 1
    printf "Usage: trace_branch <target> [max_steps]\n"
    return
  end

  set $target = $arg0

  if $argc > 1
    set $max = $arg1
  else
    set $max = 1000000000
  end

  #
  # Main trace loop
  #

  set $last_src = 0xFFFFFFFF
  set $last_dst = 0xFFFFFFFF
  set $i = 0

  printf "Tracing until PC == 0x%08x (max %d steps)\n", $target, $max

  while ($pc != $target && $i < $max)
    set $prev = $pc
    stepi_silent
    set $i = $i + 1

    if ($pc != $prev + 4)

      # filter duplicate spam
      if !($prev == $last_src && $pc == $last_dst)

        # filter trivial loops
        if ($pc != $prev)
          printf "Branch: 0x%08x -> 0x%08x\n", $prev, $pc
          set $last_src = $prev
          set $last_dst = $pc
        end

      end
    end
    
  end

  #
  # Ending message
  #

  if ($pc == $target)
    printf "Reached target at PC = 0x%08x after %d steps\n", $pc, $i
  else
    printf "Stopped after %d steps at PC = 0x%08x\n", $i, $pc
  end

end


