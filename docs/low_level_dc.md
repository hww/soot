# DC Script & Data Compilation

Примеры ниже иллюстрируют концепции, а не финальный синтаксис; см. архитектурную заметку в конце

## Data Definitions

Let’s define a player start position:

```lisp
(define-export *player-start*
    (new locator
        :trans *origin*
        :rot (axis-angle->quaternion *y-axis* 45)))
```

The DC language allows you to declare new types, below is an example of a four-
component vector

```lisp
;; Start with some types
(deftype vec4 (:align 16)
    ((x float)
     (y float)
     (z float)
     (w float :default 0)))
```

```cpp
;; C++ definition or result of some processing previous. 
struct Vec4
{
    float m_x;
    float m_y;
    float m_z;
    float m_w;
};
```

### Inheritance

You can use inheritance when declaring, example below.

```lisp
(deftype quaternion (:parent vec4)
    ())

(deftype point (:parent vec4)
    ((w float :default 1)))          ;; redefine default for W
```

Another example, but now with composition of classes.

```lisp
(deftype locator ()
    ((trans point :inline #t)        ;; inline for member mean what?
     (rot quaternion :inline #t)))
```

As a result, the DC compiler converts the structure into the contents of the .h file

```cpp
;; What it should be defined or result of processing

struct Locator
{
    Point m_trans;
    Quaternion m_rot;
};
```


## Instances

A data definition can use a function as the value. An example of definition for the player’s
15CommentHighlight
starting point is shown below. Here a function calculating quaternion from an angle and
rotation axis and the result is used as the value of the rotation angle

```lisp
;; Define some instances

(define *y-axis* (new vec4 :x 0 :y 1 :z 0))
(define *origin* (new point :x 0 :y 0 :z 0))

```

## C++ Usage

```cpp
;;; How we use these definitions in C++ code

...
#include "dc-types.h"
...
const Locator * pLoc = DcLookupSymbol("*player-start*");
Point pos = pLoc->m_trans;
... 
```


## Functions

A data definition can use a function as the value. An example of definition for the player’s
starting point is shown below. 

```lisp
(define-export *player-start*
    (new locator
        :trans *origin*
        :rot (axis-angle->quaternion *y-axis* 45)
    )
)
```

Here a function calculating quaternion from an angle and
rotation axis and the result is used as the value of the rotation angle 

```lisp
;; Define a function
(define (axis-angle->quat axis angle)
    (let ((sin-angle/2 (sin (* 0.5 angle))))
        (new quaternion
            :x (* (-> axis x) sin-angle/2)
            :y (* (-> axis y) sin-angle/2)
            :z (* (-> axis z) sin-angle/2)
            :w (cos (* 0.5 angle))
        )
    )
)
```

## Animation states 

Animation states are implemented as data structures. A corresponding C code is required
in order to interpret these states and to construct the necessary objects in the system
memory. Below is a simple animation’s statepirate-jump [Gre17].

```lisp
(define-state simple
    :name "pirate-jump"
    :clip "pirate-jump"
    :flags (anim-state-flag no-adjust-to-ground)
)
```

An example of a complex animation state is given below [Gre17]. In this case a linear
interpolation of two animations is performed: pirate-jump and pirate-scare.

```lisp
(define-state complex
    :name "pirate-jump"
    :tree
    (anim-node-lerp
    (anim-node-clip "pirate-jump")
    (anim-node-clip "pirate-scare")
    ))
```

Another example is given below, it has a tree of different nodes that perform animation
mixing operations [Gre17].

```lisp
(define-state complex
    :name "pirate-jump"
    :tree
    (anim-node-lerp
        (anim-node-additive
            (anim-node-additive
                (anim-node-clip "pirate-jump-f")
                (anim-node-clip "pirate-scare-f")
            )
            (anim-node-clip "pirate-felldown-f")
        )
        (anim-node-additive
            (anim-node-additive
                (anim-node-clip "pirate-jump-b")
                (anim-node-clip "pirate-scare-b")
            )
            (anim-node-clip "pirate-felldown-b")
        )
    )
)
```

Yet another example is given below, it has a tree different nodes that perform animation
mixing operations [Gre17].

```lisp
;; nb aim-tree is the macro definition
(define-state complex
    :name "s-turret-idle"
    :tree (aim-tree (anim-node-clip "turret-aim-all-base")
             "turret-aim-all-left-right"
             "turret-aim-all-left-updown")
    :transitions (
        (transition "reload" "s_turret-reload"
            (range - -) :fade-time 0.2)
        (transition "step-left" "s_turret-step-left"
            (range - -) :fade-time 0.2)
        (transition "step-right" "s_turret-ste-right"
            (range - -) :fade-time 0.2)
        (transition "reload" "s_turret-fire"
            (range - -) :fade-time 0.1)

        ;; invoke previously defined group of transitions
        ;; it is used when the same set of transitions needed
        ;; to be used in the other state
        (transition-group "combat-gunpout-idle-mode")

        ;; specifies a transition that is
        ;; taken upon reaching the end of the state's
        ;; local time line if no other transition
        ;; has been taken before then
        (transition-end "s-turret-idle")
    )
)
```

Similar methods can be used to encode other game systems: AI, Melee

## Meley system examples

Few more compicated examples

```llisp
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
```

## State Scripting

Пример ниже демонстрирует: 

* объявление state-script;
* работу с переменными (declarations);
* многоудачность (tracks);
* ожидание сигналов (wait-for-signal);
* переходы состояний (go);

```lisp
;; =====================================================================
;; ЕДИНЫЙ ПРИМЕР: Сцена с аварией автобуса и интерактивными воротами
;; 
;; Демонстрирует:
;;   1. Объявление state-script и его начального состояния
;;   2. Объявление локальных переменных состояния (declarations)
;;   3. Многоудачность (tracks) — параллельные процессы внутри состояния
;;   4. Ожидание сигналов между треками (wait-for-signal)
;;   5. Переходы между состояниями (go)
;;   6. Отключение управления игроком
;; =====================================================================

;; ---------------------------------------------------------------------
;; 1. Определение скрипта состояний (state-script)
;;    "kickable-gate" — это объект, у которого есть несколько состояний.
;;    :initial-state указывает, с какого состояния начинается работа.
;;    :declarations содержит переменные, доступные всем трекам этого скрипта.
;; ---------------------------------------------------------------------
(define-state-script ("kickable-gate")
    :initial-state "closed"

    ;; --- Объявление переменных состояния ---
    ;; num-attempts — счётчик попыток открыть/сломать ворота
    ;; is-locked    — флаг, заблокированы ли ворота
    :declarations (decl-list
        (var "num-attempts" :type int32)
        (var "is-locked" :default #t)
    )

    ;; =================================================================
    ;; СОСТОЯНИЕ 1: "closed" — ворота закрыты
    ;; =================================================================
    (state ("closed")
        (on (begin)
            ;; Инициализация при входе в состояние:
            ;; ворота закрыты, попыток ещё не было
            [set-var "is-locked" #t]
            [set-var "num-attempts" 0]
        )

        ;; --- Переходы из состояния "closed" ---
        ;; Если игрок взаимодействует с воротами — перейти в "opening"
        (transition "interact" "opening" (range - -) :fade-time 0.2)

        ;; Если ворота сломаны — перейти в "broken"
        (transition "break" "broken" (range - -) :fade-time 0.1)
    )

    ;; =================================================================
    ;; СОСТОЯНИЕ 2: "opening" — ворота открываются
    ;; =================================================================
    (state ("opening")
        (on (begin)
            ;; Увеличиваем счётчик попыток
            [set-var "num-attempts" (+ [get-var "num-attempts"] 1)]

            ;; Если ворота заперты — возвращаемся в "closed"
            (if [get-var "is-locked"]
                (begin
                    [go "closed"]
                )
                (begin
                    ;; Иначе запускаем анимацию открытия
                    [animate "gate" "gate-open" (get-locator "ref-gate")]
                )
            )
        )

        ;; --- Переходы из состояния "opening" ---
        ;; По завершении анимации — перейти в "open"
        (transition-end "open")
    )

    ;; =================================================================
    ;; СОСТОЯНИЕ 3: "open" — ворота открыты
    ;; =================================================================
    (state ("open")
        (on (begin)
            ;; Ворота больше не заперты
            [set-var "is-locked" #f]
        )
    )

    ;; =================================================================
    ;; СОСТОЯНИЕ 4: "broken" — ворота сломаны
    ;; =================================================================
    (state ("broken")
        (on (begin)
            ;; Проигрываем анимацию разрушения
            [animate "gate" "gate-break" (get-locator "ref-gate")]
        )
    )
)
```

Пример с паралельными треками которые позволяью синхронизировать персонажи и выполнять код с точностью кадра.

```lisp
;; =====================================================================
;; СЦЕНА С АВАРИЕЙ АВТОБУСА
;; Демонстрирует многоудачность (tracks) и синхронизацию через сигналы.
;; =====================================================================
(define-state-script ("wz-bus-crash")
    :initial-state "spawn-soldiers"

    ;; =================================================================
    ;; СОСТОЯНИЕ: "spawn-soldiers" — создание солдат
    ;; =================================================================
    (state ("spawn-soldiers")
        (on (begin)
            ;; Отключаем управление игроком, кроме правой кнопки мыши
            [player-disable-controls (controls all-but-right-stick)]

            ;; Создаём солдат в боевом режиме
            [spawn-npc-in-combat "npc-wz-52"]
            [spawn-npc-in-combat "npc-wz-53"]

            ;; Переходим в состояние "crash"
            [go "crash"]
        )
    )

    ;; =================================================================
    ;; СОСТОЯНИЕ: "crash" — сама авария
    ;; Здесь запускаются 4 параллельных трека (tracks).
    ;; Каждый трек — это отдельный процесс, который может
    ;; приостанавливаться и ждать событий.
    ;; =================================================================
    (state ("crash")
        (on (begin)

            ;; --- ТРЕК 1: Анимация автобуса ---
            (track ("bus")
                ;; Ждём завершения анимации аварии автобуса
                [wait-animate "bus-1" "bus-crash"
                    [get-locator "ref-bus-crash-1"]]
                ;; Посылаем сигнал о завершении
                [signal "bus-done"]
            )

            ;; --- ТРЕК 2: Анимация игрока ---
            (track ("player")
                ;; Игрок смотрит на место аварии
                [animate "player" "player-watch-crash"
                    [get-locator "ref-bus-crash-1"]]
                ;; Ждём 250 кадров
                [wait-until-frame 250]
                ;; Игрок произносит реплику
                [say "player" "vox-wz-drk-01-what-the"]
                ;; Посылаем сигнал
                [signal "drake-done"]
            )

            ;; --- ТРЕК 3: Анимация персонажа, сбитого автобусом ---
            (track ("guy-hit-by-bus")
                ;; Ждём анимации попадания под автобус
                [wait-animate "npc-wz-52" "npc-hit-by-bus"
                    [get-locator "ref-bus-crash-1"]]
                ;; NPC умирает
                [npc-die "npc-wz-52"]
                ;; Посылаем сигнал
                [signal "npc-dead"]
            )

            ;; --- ТРЕК 4: Ожидание завершения всех остальных треков ---
            (track ("wait-for-all-done")
                ;; Ждём сигналы от всех трёх треков
                [wait-for-signal "bus-done"]
                [wait-for-signal "drake-done"]
                [wait-for-signal "npc-dead"]
                ;; Когда все сигналы получены — переходим в "done"
                [go "done"]
            )
        )
    )

    ;; =================================================================
    ;; СОСТОЯНИЕ: "done" — сцена завершена
    ;; =================================================================
    (state ("done")
        (on (begin)
            ;; Возвращаем управление игроку
            [player-enable-controls]
            ;; Здесь могут быть другие действия по завершению сцены
        )
    )
)
```

### Reflection

The DC source code compiled into bytecode4. Any way to integrate a dynamic language
into the system requires a mechanism for this integration: Reflection, FFI, etc.
ND has a very simple but very effective way to integrate the virtual machine and the
engine itself. To do this, they use a hash table with a function name as key ssid and a
function pointer as value. This function with variable number of arguments, which have
variant type.
An example of such a function is given below [Gre06]. Object names in the form StringId
are used to access scene objects, with the reserved name self addressing the process host
object.

```cpp
Variant ScriptWaitAnimate(int argc, Variant* argv)
{
    StringId objName = SC_ARG(0,StringId, NULL);
    StringId animName = SC_ARG(1,StringId, NULL);
    if(!objName)
        // The ScriptError is a function return Variant(false)
        // And print the error message
        return ScriptError("wait-animate: expected object name (arg1)\n");
    if(!animName)
        return ScriptError("wait-animate: expect animation name (arg2)\n");
    // find the object
    ProcessGameObject* pObj = g_processMgr.Lookup(objName);
    if(!pObj)
        return ScriptError("wait-animate: could not found %s\n", StringIdToString(onjName));
    // insruct object to play animation, and wakeup
    // this script when done
    pObj->WaitAnimate(animName, g_scriptContext);
    g_scriptContext.Suspend(); // go to sleep until animation complete
    return Variant(true);
}
```

The C function ScriptWaitAnimate can now be declared in a dynamic programming
environment, see example below [Gre06]. The declaration is only needed to exposing the
method’s signature, that is, to check types.

```lisp
(define-c-function wait-animate
    (object-name string)
    (anim-name string)
)
```

## Assembly

Below is the source code of the vector-scale function. You may notice that the compiler
has no means for quality optimization. But this is not a problem, because the game has
a good architecture and a clear separation between high-intensity processes and game
logic runs on VM.

```asm
vector-scale(scalar, vector*)
    move r0, r24 ; r0 = r24 = scalar value
    move r1, r25 ; r1 = r25 = vector pointer
    ; Get native function pointer
    lookupPointer r2, data[0] ; r2 = StringId(0xcd4b9c1b)
    ; Scale X
    move r3, r0 ; r3 = r0
    move r4, r1 ; r4 = r1 = &vector.x
    loadFloat r4, (r4) ; r4 = *r4 = vector.x
    floatMul r3, r3, r4 ; r3 = r3 * r4 = x * scale
    ; Scale Y
    move r4, r0 ; r4 = r0 = scalar
    move r5, r1 ; r5 = r1 = &vector
    intAddImm r5, r5, 0x04; r5 = r5 + 4 = &vector.y
    loadFloat r5, (r5) ; r5 = *r5 = vector.y
    floatMul r4, r4, r5 ; r4 = r4 * r5 = y * scale
    ; Scale Z
    move r5, r0 ; r5 = r0 = scalar
    move r6, r1 ; r6 = r1 = &vector
    intAddImm r6, r6, 0x08; r6 = r6 + 8 = &vector.z
    loadFloat r6, (r6) ; r6 = *r6 = vector.z
    floatMul r5, r5, r6 ; r5 = r5 * r6 = vector.z * scale
    loadStaticFloat r6, data[1] ; r6 = 1
    ; Call function with 4 values
    move r24, r3 ; r24 = r3 = x
    move r25, r4 ; r25 = r4 = y
    move r26, r5 ; r26 = r5 = z
    move r27, r6 ; r27 = r6 = w = 1
    callFf r2, r2, 4 ; function(x,y,z,w)
    return r2, r2
```

## Пояснения к ключевым особенностям:

1. **`define-state-script`** — объявляет скрипт состояний для объекта (ворота, сцена).
2. **`:initial-state`** — указывает, с какого состояния начинается выполнение.
3. **`:declarations` / `decl-list`** — объявление локальных переменных состояния (`var`), доступных всем трекам.
4. **`(state ("name") ...)`** — объявление конкретного состояния.
5. **`(on (begin) ...)`** — код инициализации, выполняемый при входе в состояние.
6. **`(track ("name") ...)`** — параллельный процесс внутри состояния. Треки выполняются независимо друг от друга.
7. **`[wait-animate ...]`** — приостанавливает трек до завершения анимации.
8. **`[wait-until-frame N]`** — приостанавливает трек до наступления кадра N.
9. **`[wait-for-signal "..."]`** — приостанавливает трек до получения сигнала от другого трека.
10. **`[signal "..."]`** — посылает сигнал другим трекам.
11. **`[go "..."]`** — переход в другое состояние.
12. **`(transition ...)`** и **`(transition-end ...)`** — объявление переходов между состояниями (по событию или по завершении таймлайна).

Вот готовый раздел. Название — «Архитектурная заметка»; если не понравится, легко переименовать.

---

## Архитектурная заметка

### О двух слоях компилятора

Компилятор, который мы строим, двухслойный.

**Нижний слой — типовая основа.** Всё, что касается системы типов: объявление типов (`deftype`), различие `structure` и `basic`, методы типов, конструкторы (`new`, `object-new`), layout-полей, runtime-типы, приведение типов, — берётся из OpenGOAL. Эта система уже реализована, проверена на декомпилированном коде Jak 1 и покрывает всё, что нам нужно на уровне описания данных. Мы её не переизобретаем и не модифицируем: если OpenGOAL что-то покрывает, мы берём это оттуда как есть.

**Верхний слой — поведенческая надстройка.** Всё, что касается описания поведения поверх типов: state script, состояния, tracks, declarations, ожидание сигналов (`wait-for-signal`), переходы (`transition`, `transition-end`), деревья анимаций (`:tree`, `anim-node-lerp`, `aim-tree`), декларативные дескрипторы боевых действий (`new melee-attack`, `new melee-attack-behavior`), — приходит из DC (система из Uncharted). Это не система типов; это язык описания поведения, который опирается на типы нижнего слоя.

Оба слоя не конкурируют, а дополняют друг друга. `define-state-script`, `define-state`, `new melee-attack` в конечном счёте компилируются в конструкции нижнего слоя: типы, поля, конструкторы, методы. Расширяя поведенческий слой, мы не трогаем типовой; изменения в типовом слое автоматически подхватываются надстройкой.

### О статусе примеров в этом документе

Примеры DC-синтаксиса в первой части — это **не буквальная спецификация** того, что мы будем реализовывать. Они собраны из разных источников (в том числе презентационных материалов) и представляют собой наброски, иллюстрирующие концепции, а не финальный синтаксис.

Из этого следуют два практических правила.

Первое: **мы повторяем не по букве, а по духу.** Важна выразительная способность — какие конструкции должны быть выразимы в нашем языке, — а не конкретные ключевые слова, порядок аргументов или форма записи. Если в DC-примере написано `(track ("bus") ...)`, это означает «должна существовать возможность описать параллельную ветку поведения внутри состояния», а не «мы обязаны ввести именно форму `track` с именно таким синтаксисом».

Второе: **отдельные примеры могут быть нереалистичными или неполными.** Часть из них — упрощения, часть — ранние варианты, часть — фрагменты без контекста. Мы не обязаны разрешать противоречия между примерами и не обязаны доводить их до консистентного вида. Пример служит опорой для обсуждения, а не эталоном.

Финальный синтаксис верхнего слоя будет определён отдельно, с учётом того, что нижний слой уже даёт OpenGOAL, и с учётом того, что поведенческий слой должен быть выразительным, а не буквально копирующим DC.

---
