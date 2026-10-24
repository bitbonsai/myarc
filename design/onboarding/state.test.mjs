import test from 'node:test'
import assert from 'node:assert/strict'
import { initialState, makeSample, transition } from './state.mjs'

const apply = (state, ...events) => events.reduce(transition, state)
const event = type => ({ type })
const preview = () => apply(initialState(), event('begin'), event('review'))
const practice = () => apply(preview(), event('apply'), event('applied'), event('practice'))

test('reveal requires explicit successful sample application', () => {
  const state = preview()
  assert.equal(transition(state, event('applied')), state)
  assert.equal(transition(state, event('practice')), state)
  const pending = transition(state, event('apply'))
  assert.equal(pending.scene, 'commit')
  assert.equal(pending.committed, false)
  const ready = transition(pending, event('applied'))
  assert.equal(ready.scene, 'reveal')
  assert.equal(ready.committed, true)
})

test('failed application returns to recoverable preview, with no reveal', () => {
  const state = apply(preview(), event('apply'), event('failed'))
  assert.equal(state.scene, 'preview')
  assert.equal(state.committed, false)
  assert.match(state.error, /unchanged/)
  assert.equal(transition(state, event('skip')), state)
  const retried = apply(state, event('apply'), event('applied'))
  assert.equal(retried.scene, 'reveal')
  assert.equal(retried.error, null)
})

test('back preserves sample choice; start fresh reaches empty reveal immediately', () => {
  const source = apply(initialState(), event('begin'), { type: 'sample', sample: 'large' })
  const back = apply(source, event('review'), event('back'))
  assert.equal(back.sample, 'large')
  assert.equal(back.scene, 'source')
  const fresh = transition(back, event('fresh'))
  assert.equal(fresh.scene, 'reveal')
  assert.equal(makeSample(fresh.sample).pins.length, 0)
  assert.equal(transition(fresh, event('skip')).scene, 'complete')
})

test('practice records actual local interactions; skipping does not mark success', () => {
  let state = practice()
  const invalid = transition(state, { type: 'pin', id: 'pin-1' })
  assert.equal(invalid, state)
  state = transition(state, { type: 'pin', id: 'pin-0' })
  assert.deepEqual(state.completed, [0])
  state = transition(state, { type: 'space', id: 'weekend' })
  assert.deepEqual(state.completed, [0, 1])
  state = transition(state, event('command'))
  assert.deepEqual(state.completed, [0, 1, 2])
  assert.equal(transition(state, event('skip')).scene, 'complete')
  const skipped = apply(practice(), event('next'), event('next'), event('next'))
  assert.equal(skipped.scene, 'complete')
  assert.deepEqual(skipped.completed, [])
})

test('synthetic samples have stable identities and shared Profile references', () => {
  const sample = makeSample('large')
  assert.equal(sample.pins.length, 1000)
  assert.equal(new Set(sample.pins.map(pin => pin.id)).size, 1000)
  assert.deepEqual(makeSample('large'), sample)
  assert.equal(sample.spaces[0].profile, sample.spaces[1].profile)
  assert.notEqual(sample.spaces[0].profile, sample.spaces[2].profile)
  assert.equal(makeSample('empty').pins.length, 0)
  assert.throws(() => makeSample('untrusted'), /Unknown study sample/)
})

test('unknown inputs and out-of-order actions cannot bypass scene guards', () => {
  const start = initialState()
  for (const type of ['applied', 'practice', 'apply', 'next', 'skip', 'command', 'pin', 'space', 'unknown']) {
    assert.equal(transition(start, { type }), start)
  }
  const source = transition(start, event('begin'))
  assert.equal(transition(source, { type: 'sample', sample: '<script>' }), source)
  assert.deepEqual(transition(practice(), event('replay')), start)
})
