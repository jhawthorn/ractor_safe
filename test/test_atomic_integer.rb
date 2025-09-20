require 'test_helper'

class TestAtomicInteger < Minitest::Test
  def test_initialization
    ai = RactorSafe::AtomicInteger.new
    assert_equal 0, ai.value

    ai = RactorSafe::AtomicInteger.new(42)
    assert_equal 42, ai.value
  end

  def test_basic_operations
    ai = RactorSafe::AtomicInteger.new(10)

    assert_equal 10, ai.value

    ai.value = 20
    assert_equal 20, ai.value
  end

  def test_increment_decrement
    ai = RactorSafe::AtomicInteger.new(5)

    assert_equal 6, ai.increment
    assert_equal 6, ai.value

    assert_equal 5, ai.decrement
    assert_equal 5, ai.value
  end

  def test_add_subtract
    ai = RactorSafe::AtomicInteger.new(10)

    assert_equal 15, ai.add(5)
    assert_equal 15, ai.value

    assert_equal 12, ai.subtract(3)
    assert_equal 12, ai.value
  end

  def test_compare_and_set
    ai = RactorSafe::AtomicInteger.new(10)

    assert ai.compare_and_set(10, 20)
    assert_equal 20, ai.value

    refute ai.compare_and_set(10, 30)
    assert_equal 20, ai.value
  end

  def test_shareable_between_ractors
    ai = RactorSafe::AtomicInteger.new(0)

    assert Ractor.shareable?(ai)

    # Test concurrent increments
    ractors = 10.times.map do
      Ractor.new(ai) do |atomic_int|
        100.times { atomic_int.increment }
      end
    end

    ractors.each(&:value)

    assert_equal 1000, ai.value
  end

  def test_concurrent_compare_and_set
    ai = RactorSafe::AtomicInteger.new(0)

    # Multiple ractors trying to set the same value
    ractors = 10.times.map do |i|
      Ractor.new(ai, i) do |atomic_int, id|
        success = atomic_int.compare_and_set(0, id + 1)
        [id, success]
      end
    end

    results = ractors.map(&:value)
    successful_sets = results.count { |_, success| success }

    # Only one should succeed
    assert_equal 1, successful_sets

    # Value should be between 1 and 10
    assert_includes 1..10, ai.value
  end

  def test_negative_numbers
    ai = RactorSafe::AtomicInteger.new(-5)

    assert_equal(-5, ai.value)

    assert_equal(-4, ai.increment)
    assert_equal(-5, ai.decrement)
    assert_equal(-10, ai.subtract(5))
    assert_equal(-5, ai.add(5))
  end
end