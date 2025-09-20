# RactorSafe

> [!CAUTION]
> This is an experiment to fill in where built-in functionality is inconvenient. All APIs here may change, be removed, or be moved into a different project or upstream.

Experimental Ractor-safe data structures for Ruby.

At the moment the implementation is focused on simplicity rather than performance.

## Installation

Install the gem and add to the application's Gemfile by executing:

```bash
bundle add ractor_safe
```

## Usage

RactorSafe provides thread-safe, Ractor-shareable data structures:

### Queue
```ruby
queue = RactorSafe::Queue.new
queue.push(42) # must be shareable
queue.pop # => 42
```

### HashMap
```ruby
map = RactorSafe::HashMap.new
map[:key] = "value".freeze # key/value must be shareable
map[:key] # => "value"
```

### AtomicInteger
```ruby
counter = RactorSafe::AtomicInteger.new(0)
counter.increment # => 1
counter.add(5) # => 6
```

## Development

After checking out the repo, run `bin/setup` to install dependencies. Then, run `rake test` to run the tests. You can also run `bin/console` for an interactive prompt that will allow you to experiment.

To install this gem onto your local machine, run `bundle exec rake install`. To release a new version, update the version number in `version.rb`, and then run `bundle exec rake release`, which will create a git tag for the version, push git commits and the created tag, and push the `.gem` file to [rubygems.org](https://rubygems.org).

## Contributing

Bug reports and pull requests are welcome on GitHub at https://github.com/jhawthorn/ractor_safe. This project is intended to be a safe, welcoming space for collaboration, and contributors are expected to adhere to the [code of conduct](https://github.com/jhawthorn/ractor_safe/blob/main/CODE_OF_CONDUCT.md).

## License

The gem is available as open source under the terms of the [MIT License](https://opensource.org/licenses/MIT).

## Code of Conduct

Everyone interacting in the RactorSafe project's codebases, issue trackers, chat rooms and mailing lists is expected to follow the [code of conduct](https://github.com/jhawthorn/ractor_safe/blob/main/CODE_OF_CONDUCT.md).
