(new melee-attack
    :anim 'swing-attack
    :start-func
        (and
            (characters-in-range? 3.0)
            (is-in-front?)
            (has-line-of-motion?)
        )
    :end-func
        (target-out-of-range? 5.0)
        (line-of-motion-blocked?)
    :events
        (make-event-list
            (npc-track-target)
            (avoid-overshoot-event)
        )
)

(new melee-attack-behavior
    :name 'attack-basic-tell-close-combo
    :attack-list 'npc-basic-tell-close-attacks
    :test-hit-frame-overlap #t
    :destination (melee-destination target)
    :range (range - -)
    :time-since-last-attack-ended (range 0.5 -)
    :range-hysteresis-upper 2.0
    :cooldown-npc 1.0
    :cooldown-global 0.0
    :num-ally-attackers-in-circle (range - 2)
    :start-func (...)
    :end-func (...)
    :update-func (...)
    :enter-func (...)
    :exit-func (...)
)